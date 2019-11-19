// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/printer_capabilities.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "content/public/test/browser_task_environment.h"
#include "printing/backend/test_print_backend.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

using base::Value;

namespace {

const char kDpi[] = "dpi";

void GetSettingsDone(base::OnceClosure done_closure,
                     base::Value* out_settings,
                     base::Value settings) {
  *out_settings = std::move(settings);
  std::move(done_closure).Run();
}

}  // namespace

class PrinterCapabilitiesTest : public testing::Test {
 public:
  PrinterCapabilitiesTest() = default;
  ~PrinterCapabilitiesTest() override = default;

 protected:
  void SetUp() override {
    test_backend_ = base::MakeRefCounted<TestPrintBackend>();
    PrintBackend::SetPrintBackendForTesting(test_backend_.get());
    blocking_task_runner_ = base::CreateSingleThreadTaskRunner(
        {base::ThreadPool(), base::MayBlock()});
    disallow_blocking_ = std::make_unique<base::ScopedDisallowBlocking>();
  }

  void TearDown() override {
    disallow_blocking_.reset();
    test_backend_.reset();
  }

  base::Value GetSettingsOnBlockingTaskRunnerAndWaitForResults(
      const std::string& printer_name,
      const PrinterBasicInfo& basic_info,
      const PrinterSemanticCapsAndDefaults::Papers& papers,
      scoped_refptr<PrintBackend> backend) {
    base::RunLoop run_loop;
    base::Value settings;

    base::PostTaskAndReplyWithResult(
        blocking_task_runner_.get(), FROM_HERE,
        base::BindOnce(&GetSettingsOnBlockingTaskRunner, printer_name,
                       basic_info, papers, /*has_secure_protocol=*/false,
                       backend),
        base::BindOnce(&GetSettingsDone, run_loop.QuitClosure(), &settings));

    run_loop.Run();
    return settings;
  }

  TestPrintBackend* print_backend() { return test_backend_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<TestPrintBackend> test_backend_;
  scoped_refptr<base::TaskRunner> blocking_task_runner_;
  std::unique_ptr<base::ScopedDisallowBlocking> disallow_blocking_;
};

// Verify that we don't crash for a missing printer and a nullptr is never
// returned.
TEST_F(PrinterCapabilitiesTest, NonNullForMissingPrinter) {
  std::string printer_name = "missing_printer";
  PrinterBasicInfo basic_info;
  PrinterSemanticCapsAndDefaults::Papers no_additional_papers;

  base::Value settings_dictionary =
      GetSettingsOnBlockingTaskRunnerAndWaitForResults(
          printer_name, basic_info, no_additional_papers, nullptr);

  ASSERT_FALSE(settings_dictionary.DictEmpty());
}

TEST_F(PrinterCapabilitiesTest, ProvidedCapabilitiesUsed) {
  std::string printer_name = "test_printer";
  PrinterBasicInfo basic_info;
  PrinterSemanticCapsAndDefaults::Papers no_additional_papers;

  // Set a capability and add a valid printer.
  auto caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
  caps->dpis = {{600, 600}};
  print_backend()->AddValidPrinter(printer_name, std::move(caps));

  base::Value settings_dictionary =
      GetSettingsOnBlockingTaskRunnerAndWaitForResults(
          printer_name, basic_info, no_additional_papers, print_backend());

  // Verify settings were created.
  ASSERT_FALSE(settings_dictionary.DictEmpty());

  // Verify capabilities dict exists and has 2 entries. (printer and version)
  base::Value* cdd = settings_dictionary.FindKeyOfType(
      kSettingCapabilities, base::Value::Type::DICTIONARY);
  ASSERT_TRUE(cdd);
  EXPECT_EQ(2U, cdd->DictSize());

  // Read the CDD for the "dpi" attribute.
  base::Value* caps_dict =
      cdd->FindKeyOfType(kPrinter, base::Value::Type::DICTIONARY);
  ASSERT_TRUE(caps_dict);
  EXPECT_TRUE(caps_dict->FindKey(kDpi));
}

// Ensure that the capabilities dictionary is present but empty if the backend
// doesn't return capabilities.
TEST_F(PrinterCapabilitiesTest, NullCapabilitiesExcluded) {
  std::string printer_name = "test_printer";
  PrinterBasicInfo basic_info;
  PrinterSemanticCapsAndDefaults::Papers no_additional_papers;

  // Return false when attempting to retrieve capabilities.
  print_backend()->AddValidPrinter(printer_name, nullptr);

  base::Value settings_dictionary =
      GetSettingsOnBlockingTaskRunnerAndWaitForResults(
          printer_name, basic_info, no_additional_papers, print_backend());

  // Verify settings were created.
  ASSERT_FALSE(settings_dictionary.DictEmpty());

  // Verify that capabilities is an empty dictionary.
  base::Value* caps_dict = settings_dictionary.FindKeyOfType(
      kSettingCapabilities, base::Value::Type::DICTIONARY);
  ASSERT_TRUE(caps_dict);
  EXPECT_TRUE(caps_dict->DictEmpty());
}

TEST_F(PrinterCapabilitiesTest, AdditionalPapers) {
  std::string printer_name = "test_printer";
  PrinterBasicInfo basic_info;

  // Set a capability and add a valid printer.
  auto caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
  caps->dpis = {{600, 600}};
  print_backend()->AddValidPrinter(printer_name, std::move(caps));

  // Add some more paper sizes.
  PrinterSemanticCapsAndDefaults::Papers additional_papers;
  additional_papers.push_back({"foo", "vendor", {200, 300}});
  additional_papers.push_back({"bar", "vendor", {600, 600}});

  base::Value settings_dictionary =
      GetSettingsOnBlockingTaskRunnerAndWaitForResults(
          printer_name, basic_info, additional_papers, print_backend());

  // Verify settings were created.
  ASSERT_FALSE(settings_dictionary.DictEmpty());

  // Verify there is a CDD with a printer entry.
  const Value* cdd = settings_dictionary.FindKeyOfType(kSettingCapabilities,
                                                       Value::Type::DICTIONARY);
  ASSERT_TRUE(cdd);
  const Value* printer = cdd->FindKeyOfType(kPrinter, Value::Type::DICTIONARY);
  ASSERT_TRUE(printer);

  // Verify there are 2 paper sizes.
  const Value* media_size =
      printer->FindKeyOfType("media_size", Value::Type::DICTIONARY);
  ASSERT_TRUE(media_size);
  const Value* media_option =
      media_size->FindKeyOfType("option", Value::Type::LIST);
  ASSERT_TRUE(media_option);
  const auto& list = media_option->GetList();
  ASSERT_EQ(2U, list.size());
  ASSERT_TRUE(list[0].is_dict());
  ASSERT_TRUE(list[1].is_dict());

  // Verify the 2 paper sizes are the ones in |additional_papers|.
  const Value* name;
  const Value* vendor;
  const Value* width;
  const Value* height;

  name = list[0].FindKeyOfType("custom_display_name", Value::Type::STRING);
  ASSERT_TRUE(name);
  EXPECT_EQ("foo", name->GetString());
  vendor = list[0].FindKeyOfType("vendor_id", Value::Type::STRING);
  ASSERT_TRUE(vendor);
  EXPECT_EQ("vendor", vendor->GetString());
  width = list[0].FindKeyOfType("width_microns", Value::Type::INTEGER);
  ASSERT_TRUE(width);
  EXPECT_EQ(200, width->GetInt());
  height = list[0].FindKeyOfType("height_microns", Value::Type::INTEGER);
  ASSERT_TRUE(height);
  EXPECT_EQ(300, height->GetInt());

  name = list[1].FindKeyOfType("custom_display_name", Value::Type::STRING);
  ASSERT_TRUE(name);
  EXPECT_EQ("bar", name->GetString());
  vendor = list[1].FindKeyOfType("vendor_id", Value::Type::STRING);
  ASSERT_TRUE(vendor);
  EXPECT_EQ("vendor", vendor->GetString());
  width = list[1].FindKeyOfType("width_microns", Value::Type::INTEGER);
  ASSERT_TRUE(width);
  EXPECT_EQ(600, width->GetInt());
  height = list[1].FindKeyOfType("height_microns", Value::Type::INTEGER);
  ASSERT_TRUE(height);
  EXPECT_EQ(600, height->GetInt());
}

#if defined(OS_CHROMEOS)
TEST_F(PrinterCapabilitiesTest, HasNotSecureProtocol) {
  std::string printer_name = "test_printer";
  PrinterBasicInfo basic_info;
  PrinterSemanticCapsAndDefaults::Papers no_additional_papers;

  // Set a capability and add a valid printer.
  auto caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
  caps->pin_supported = true;
  print_backend()->AddValidPrinter(printer_name, std::move(caps));

  base::Value settings_dictionary =
      GetSettingsOnBlockingTaskRunnerAndWaitForResults(
          printer_name, basic_info, no_additional_papers, print_backend());

  // Verify settings were created.
  ASSERT_FALSE(settings_dictionary.DictEmpty());

  // Verify there is a CDD with a printer entry.
  const Value* cdd = settings_dictionary.FindKeyOfType(kSettingCapabilities,
                                                       Value::Type::DICTIONARY);
  ASSERT_TRUE(cdd);
  const Value* printer = cdd->FindKeyOfType(kPrinter, Value::Type::DICTIONARY);
  ASSERT_TRUE(printer);

  // Verify that pin is not supported.
  const Value* pin = printer->FindKeyOfType("pin", Value::Type::DICTIONARY);
  ASSERT_TRUE(pin);
  base::Optional<bool> pin_supported = pin->FindBoolKey("supported");
  ASSERT_TRUE(pin_supported.has_value());
  ASSERT_FALSE(pin_supported.value());
}
#endif  // defined(OS_CHROMEOS)

}  // namespace printing
