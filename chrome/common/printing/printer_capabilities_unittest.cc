// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/printing/printer_capabilities.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/printing/printing_buildflags.h"
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
  const std::string* name =
      paper_dict.GetDict().FindString("custom_display_name");
  ASSERT_TRUE(name);
  EXPECT_EQ(expected_name, *name);
  const std::string* vendor = paper_dict.GetDict().FindString("vendor_id");
  ASSERT_TRUE(vendor);
  EXPECT_EQ(expected_vendor, *vendor);
  std::optional<int> width = paper_dict.GetDict().FindInt("width_microns");
  ASSERT_TRUE(width.has_value());
  EXPECT_EQ(expected_size.width(), width.value());
  std::optional<int> height = paper_dict.GetDict().FindInt("height_microns");
  ASSERT_TRUE(height.has_value());
  EXPECT_EQ(expected_size.height(), height.value());
}

#if BUILDFLAG(IS_CHROMEOS)
void VerifyMediaType(const base::Value& media_type_dict,
                     const std::string& expected_name,
                     const std::string& expected_vendor) {
  ASSERT_TRUE(media_type_dict.is_dict());
  const std::string* name =
      media_type_dict.GetDict().FindString("custom_display_name");
  ASSERT_TRUE(name);
  EXPECT_EQ(expected_name, *name);
  const std::string* vendor = media_type_dict.GetDict().FindString("vendor_id");
  ASSERT_TRUE(vendor);
  EXPECT_EQ(expected_vendor, *vendor);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

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

  void TearDown() override { PrintBackend::SetPrintBackendForTesting(nullptr); }

  base::Value::Dict GetSettingsOnBlockingTaskRunnerAndWaitForResults(
      const std::string& printer_name,
      const PrinterBasicInfo& basic_info,
      PrinterSemanticCapsAndDefaults::Papers papers) {
    base::RunLoop run_loop;
    base::Value::Dict settings;

    blocking_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&GetSettingsOnBlockingTaskRunner, printer_name,
                       basic_info, std::move(papers), test_backend_),
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
#if BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)
  VerifyPaper((*media_option)[0], "0 x 0 mm", "om_100x234um_0x0mm", {100, 234});
#else
  VerifyPaper((*media_option)[0], "printer_foo", "printer_vendor", {100, 234});
#endif
  VerifyPaper((*media_option)[1], "foo", "vendor", {200, 300});
  VerifyPaper((*media_option)[2], "bar", "vendor", {600, 600});
}

#if BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)
TEST_F(PrinterCapabilitiesTest, PaperLocalizationsApplied) {
  std::string printer_name = "test_printer";
  PrinterBasicInfo basic_info;

  // Set a capability and add a valid printer.
  auto caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
  caps->papers.push_back(
      {"na square-photo", "oe_square-photo_5x5in", {127000, 127000}});
  caps->papers.push_back(
      {"om 21x40", "om_photo-21x40_215x400mm", {215000, 400000}});
  caps->papers.push_back({"na letter", "na_letter_8.5x11in", {215900, 279400}});
  caps->papers.push_back(
      {"na custom-2", "na_custom-2_299.6x405.3mm", {299600, 405300}});
  caps->papers.push_back(
      {"na custom-1", "na_custom-1_9x12.1in", {228600, 307340}});
  caps->papers.push_back(
      {"na index-4x6", "na_index-4x6_4x6in", {101600, 152400}});
  caps->papers.push_back(
      {"na square-photo", "oe_square-photo_4x4in", {101600, 101600}});
  caps->papers.push_back({"iso a4", "iso_a4_210x297mm", {210000, 297000}});
  caps->papers.push_back(
      {"om folio-sp", "om_folio-sp_215x315mm", {215000, 315000}});
  caps->papers.push_back({"oe photo-l", "oe_photo-l_3.5x5in", {88900, 127000}});
  caps->papers.push_back({"om folio", "om_folio_210x330mm", {210000, 330000}});
  caps->papers.push_back({"oe 2x3", "oe_2x3_2x3in", {50800, 76200}});
  caps->papers.push_back({"om 1x1", "om_1-x-1_1x1mm", {1000, 1000}});

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
  ASSERT_EQ(15U, media_option->size());

  // Verify the paper sizes are the ones in `caps->papers` in the correct
  // order.
  VerifyPaper((*media_option)[0], "2 x 3 in", "om_50800x76200um_50x76mm",
              {50800, 76200});
  VerifyPaper((*media_option)[1], "3.5 x 5 in", "oe_photo-l_3.5x5in",
              {88900, 127000});
  VerifyPaper((*media_option)[2], "4 x 4 in", "oe_square-photo_4x4in",
              {101600, 101600});
  VerifyPaper((*media_option)[3], "4 x 6 in", "na_index-4x6_4x6in",
              {101600, 152400});
  VerifyPaper((*media_option)[4], "5 x 5 in", "oe_square-photo_5x5in",
              {127000, 127000});
  VerifyPaper((*media_option)[5], "9 x 12.1 in", "om_228600x307340um_228x307mm",
              {228600, 307340});
  VerifyPaper((*media_option)[6], "1 x 1 mm", "om_1000x1000um_1x1mm",
              {1000, 1000});
  VerifyPaper((*media_option)[7], "210 x 330 mm", "om_folio_210x330mm",
              {210000, 330000});
  VerifyPaper((*media_option)[8], "215 x 315 mm", "om_folio-sp_215x315mm",
              {215000, 315000});
  VerifyPaper((*media_option)[9], "215 x 400 mm",
              "om_215000x400000um_215x400mm", {215000, 400000});
  VerifyPaper((*media_option)[10], "300 x 405 mm",
              "om_299600x405300um_299x405mm", {299600, 405300});
  VerifyPaper((*media_option)[11], "A4", "iso_a4_210x297mm", {210000, 297000});
  VerifyPaper((*media_option)[12], "Letter", "na_letter_8.5x11in",
              {215900, 279400});
  VerifyPaper((*media_option)[13], "foo", "vendor", {200, 300});
  VerifyPaper((*media_option)[14], "bar", "vendor", {600, 600});
}
#endif  // BUILDFLAG(PRINT_MEDIA_L10N_ENABLED)

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(PrinterCapabilitiesTest, MediaTypeLocalizationsApplied) {
  std::string printer_name = "test_printer";
  PrinterBasicInfo basic_info;

  // Set a capability and add a valid printer.
  auto caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
  caps->papers.push_back({"na letter", "na_letter_8.5x11in", {215900, 279400}});

  // Add some media types.
  caps->media_types.push_back({"stationery display name", "stationery"});
  caps->media_types.push_back({"custom 1 display name", "custom-1"});
  caps->media_types.push_back({"photo display name", "photographic"});
  caps->media_types.push_back({"custom 2 display name", "custom-2"});

  caps->dpis = {{600, 600}};
  print_backend()->AddValidPrinter(
      printer_name, std::move(caps),
      std::make_unique<printing::PrinterBasicInfo>(basic_info));

  base::Value::Dict settings_dictionary =
      GetSettingsOnBlockingTaskRunnerAndWaitForResults(printer_name, basic_info,
                                                       {});

  // Verify settings were created.
  ASSERT_FALSE(settings_dictionary.empty());

  // Verify there is a CDD with a printer entry.
  const base::Value::Dict* cdd =
      settings_dictionary.FindDict(kSettingCapabilities);
  ASSERT_TRUE(cdd);
  const base::Value::Dict* printer = cdd->FindDict(kPrinter);
  ASSERT_TRUE(printer);

  // Verify there are 4 media types.
  const base::Value::Dict* media_type = printer->FindDict("media_type");
  ASSERT_TRUE(media_type);
  const base::Value::List* media_type_option = media_type->FindList("option");
  ASSERT_TRUE(media_type_option);
  ASSERT_EQ(4U, media_type_option->size());

  VerifyMediaType((*media_type_option)[0], "Paper (Plain)", "stationery");
  VerifyMediaType((*media_type_option)[1], "custom 1 display name", "custom-1");
  VerifyMediaType((*media_type_option)[2], "Photo", "photographic");
  VerifyMediaType((*media_type_option)[3], "custom 2 display name", "custom-2");
}
#endif  // BUILDFLAG(IS_CHROMEOS)

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
  std::optional<bool> pin_supported = pin->FindBool("supported");
  ASSERT_TRUE(pin_supported.has_value());
  ASSERT_FALSE(pin_supported.value());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace printing
