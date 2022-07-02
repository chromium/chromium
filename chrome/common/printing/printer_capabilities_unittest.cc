// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/printing/printer_capabilities.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_runner_util.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "content/public/test/browser_task_environment.h"
#include "printing/backend/test_print_backend.h"
#include "printing/print_job_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace {

const char kDpi[] = "dpi";

void GetSettingsDone(base::OnceClosure done_closure,
                     base::Value::Dict* out_settings,
                     base::Value::Dict settings) {
  *out_settings = std::move(settings);
  std::move(done_closure).Run();
}

void VerifyPaper(const base::Value& paper_dict,
                 const std::string& expected_name,
                 const std::string& expected_vendor,
                 const gfx::Size& expected_size) {
  ASSERT_TRUE(paper_dict.is_dict());
  const std::string* name = paper_dict.FindStringKey("custom_display_name");
  ASSERT_TRUE(name);
  EXPECT_EQ(expected_name, *name);
  const std::string* vendor = paper_dict.FindStringKey("vendor_id");
  ASSERT_TRUE(vendor);
  EXPECT_EQ(expected_vendor, *vendor);
  absl::optional<int> width = paper_dict.FindIntKey("width_microns");
  ASSERT_TRUE(width.has_value());
  EXPECT_EQ(expected_size.width(), width.value());
  absl::optional<int> height = paper_dict.FindIntKey("height_microns");
  ASSERT_TRUE(height.has_value());
  EXPECT_EQ(expected_size.height(), height.value());
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
    blocking_task_runner_ =
        base::ThreadPool::CreateSingleThreadTaskRunner({base::MayBlock()});
    disallow_blocking_ = std::make_unique<base::ScopedDisallowBlocking>();
  }

  void TearDown() override {
    disallow_blocking_.reset();
    test_backend_.reset();
  }

  base::Value::Dict GetSettingsOnBlockingTaskRunnerAndWaitForResults(
      const std::string& printer_name,
      const PrinterBasicInfo& basic_info,
      PrinterSemanticCapsAndDefaults::Papers papers) {
    base::RunLoop run_loop;
    base::Value::Dict settings;

    base::PostTaskAndReplyWithResult(
        blocking_task_runner_.get(), FROM_HERE,
        base::BindOnce(&GetSettingsOnBlockingTaskRunner, printer_name,
                       basic_info, std::move(papers),
                       /*has_secure_protocol=*/false, test_backend_),
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
  PrinterSemanticCapsAndDefaults::Papers no_user_defined_papers;

  base::Value::Dict settings_dictionary =
      GetSettingsOnBlockingTaskRunnerAndWaitForResults(
          printer_name, basic_info, std::move(no_user_defined_papers));

  ASSERT_FALSE(settings_dictionary.empty());
}

TEST_F(PrinterCapabilitiesTest, ProvidedCapabilitiesUsed) {
  std::string printer_name = "test_printer";
  PrinterBasicInfo basic_info;
  PrinterSemanticCapsAndDefaults::Papers no_user_defined_papers;

  // Set a capability and add a valid printer.
  auto caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
  caps->dpis = {{600, 600}};
  print_backend()->AddValidPrinter(
      printer_name, std::move(caps),
      std::make_unique<printing::PrinterBasicInfo>(basic_info));

  base::Value::Dict settings_dictionary =
      GetSettingsOnBlockingTaskRunnerAndWaitForResults(
          printer_name, basic_info, std::move(no_user_defined_papers));

  // Verify settings were created.
  ASSERT_FALSE(settings_dictionary.empty());

  // Verify capabilities dict exists and has 2 entries. (printer and version)
  const base::Value::Dict* cdd =
      settings_dictionary.FindDict(kSettingCapabilities);
  ASSERT_TRUE(cdd);
  EXPECT_EQ(2U, cdd->size());

  // Read the CDD for the "dpi" attribute.
  const base::Value::Dict* caps_dict = cdd->FindDict(kPrinter);
  ASSERT_TRUE(caps_dict);
  EXPECT_TRUE(caps_dict->contains(kDpi));
}

// Ensure that the capabilities dictionary is not present if the backend
// doesn't return capabilities.
TEST_F(PrinterCapabilitiesTest, NullCapabilitiesExcluded) {
  std::string printer_name = "test_printer";
  PrinterBasicInfo basic_info;
  PrinterSemanticCapsAndDefaults::Papers no_user_defined_papers;

  // Return false when attempting to retrieve capabilities.
  print_backend()->AddValidPrinter(printer_name, /*caps=*/nullptr,
                                   /*info=*/nullptr);

  base::Value::Dict settings_dictionary =
      GetSettingsOnBlockingTaskRunnerAndWaitForResults(
          printer_name, basic_info, std::move(no_user_defined_papers));

  // Verify settings were created.
  ASSERT_FALSE(settings_dictionary.empty());

  // Verify that capabilities is not present.
  ASSERT_FALSE(settings_dictionary.contains(kSettingCapabilities));
}

TEST_F(PrinterCapabilitiesTest, UserDefinedPapers) {
  std::string printer_name = "test_printer";
  PrinterBasicInfo basic_info;

  // Set a capability and add a valid printer.
  auto caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
  caps->papers.push_back({"printer_foo", "printer_vendor", {100, 234}});
  caps->dpis = {{600, 600}};
  print_backend()->AddValidPrinter(
      printer_name, std::move(caps),
      std::make_unique<printing::PrinterBasicInfo>(basic_info));

  // Add some more paper sizes.
  PrinterSemanticCapsAndDefaults::Papers user_defined_papers;
  user_defined_papers.push_back({"foo", "vendor", {200, 300}});
  user_defined_papers.push_back({"bar", "vendor", {600, 600}});

  base::Value::Dict settings_dictionary =
      GetSettingsOnBlockingTaskRunnerAndWaitForResults(
          printer_name, basic_info, std::move(user_defined_papers));

  // Verify settings were created.
  ASSERT_FALSE(settings_dictionary.empty());

  // Verify there is a CDD with a printer entry.
  const base::Value::Dict* cdd =
      settings_dictionary.FindDict(kSettingCapabilities);
  ASSERT_TRUE(cdd);
  const base::Value::Dict* printer = cdd->FindDict(kPrinter);
  ASSERT_TRUE(printer);

  // Verify there are 3 paper sizes.
  const base::Value::Dict* media_size = printer->FindDict("media_size");
  ASSERT_TRUE(media_size);
  const base::Value::List* media_option = media_size->FindList("option");
  ASSERT_TRUE(media_option);
  ASSERT_EQ(3U, media_option->size());

  // Verify the 3 paper sizes are the ones in |caps->papers|, followed by the
  // ones in |user_defined_papers|.
  VerifyPaper((*media_option)[0], "printer_foo", "printer_vendor", {100, 234});
  VerifyPaper((*media_option)[1], "foo", "vendor", {200, 300});
  VerifyPaper((*media_option)[2], "bar", "vendor", {600, 600});
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PrinterCapabilitiesTest, HasNotSecureProtocol) {
  std::string printer_name = "test_printer";
  PrinterBasicInfo basic_info;
  PrinterSemanticCapsAndDefaults::Papers no_user_defined_papers;

  // Set a capability and add a valid printer.
  auto caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
  caps->pin_supported = true;
  print_backend()->AddValidPrinter(
      printer_name, std::move(caps),
      std::make_unique<printing::PrinterBasicInfo>(basic_info));

  base::Value::Dict settings_dictionary =
      GetSettingsOnBlockingTaskRunnerAndWaitForResults(
          printer_name, basic_info, std::move(no_user_defined_papers));

  // Verify settings were created.
  ASSERT_FALSE(settings_dictionary.empty());

  // Verify there is a CDD with a printer entry.
  const base::Value::Dict* cdd =
      settings_dictionary.FindDict(kSettingCapabilities);
  ASSERT_TRUE(cdd);
  const base::Value::Dict* printer = cdd->FindDict(kPrinter);
  ASSERT_TRUE(printer);

  // Verify that pin is not supported.
  const base::Value::Dict* pin = printer->FindDict("pin");
  ASSERT_TRUE(pin);
  absl::optional<bool> pin_supported = pin->FindBool("supported");
  ASSERT_TRUE(pin_supported.has_value());
  ASSERT_FALSE(pin_supported.value());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace printing
