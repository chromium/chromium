// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_chromeos.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/chromeos/printing/test_cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/test_printer_configurer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/printing/ppd_provider.h"
#include "components/printing/browser/printer_capabilities.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/printing_restrictions.h"
#include "printing/backend/test_print_backend.h"
#include "printing/print_job_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

using chromeos::CupsPrintersManager;
using chromeos::Printer;
using chromeos::PrinterClass;
using chromeos::PrinterConfigurer;
using chromeos::PrinterSetupCallback;
using chromeos::PrinterSetupResult;

// Used as a callback to StartGetPrinters in tests.
// Increases |*call_count| and records values returned by StartGetPrinters.
void RecordPrinterList(size_t* call_count,
                       std::unique_ptr<base::ListValue>* printers_out,
                       const base::ListValue& printers) {
  ++(*call_count);
  printers_out->reset(printers.DeepCopy());
}

// Used as a callback to StartGetPrinters in tests.
// Records that the test is done.
void RecordPrintersDone(bool* is_done_out) {
  *is_done_out = true;
}

void RecordGetCapability(std::unique_ptr<base::Value>* capabilities_out,
                         base::Value capability) {
  capabilities_out->reset(capability.DeepCopy());
}

void RecordGetEulaUrl(std::string* fetched_eula_url,
                      const std::string& eula_url) {
  *fetched_eula_url = eula_url;
}

Printer CreateTestPrinter(const std::string& id,
                          const std::string& name,
                          const std::string& description) {
  Printer printer;
  printer.set_id(id);
  printer.set_display_name(name);
  printer.set_description(description);
  return printer;
}

Printer CreateTestPrinterWithPpdReference(const std::string& id,
                                          const std::string& name,
                                          const std::string& description,
                                          Printer::PpdReference ref) {
  Printer printer = CreateTestPrinter(id, name, description);
  Printer::PpdReference* mutable_ppd_reference =
      printer.mutable_ppd_reference();
  *mutable_ppd_reference = ref;
  return printer;
}

Printer CreateEnterprisePrinter(const std::string& id,
                                const std::string& name,
                                const std::string& description) {
  Printer printer = CreateTestPrinter(id, name, description);
  printer.set_source(Printer::SRC_POLICY);
  return printer;
}

// Converts JSON string to base::ListValue object.
// On failure, returns NULL and fills |*error| string.
std::unique_ptr<base::ListValue> GetJSONAsListValue(const std::string& json,
                                                    std::string* error) {
  auto ret = base::ListValue::From(
      JSONStringValueDeserializer(json).Deserialize(nullptr, error));
  if (!ret)
    *error = "Value is not a list.";
  return ret;
}

// Fake PpdProvider backend. This fake PpdProvider is used to fake fetching the
// PPD EULA license of a destination. If |effective_make_and_model| is empty, it
// will return with NOT_FOUND and an empty string. Otherwise, it will return
// SUCCESS with |effective_make_and_model| as the PPD license.
class FakePpdProvider : public chromeos::PpdProvider {
 public:
  FakePpdProvider() = default;

  void ResolvePpdLicense(base::StringPiece effective_make_and_model,
                         ResolvePpdLicenseCallback cb) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(cb),
                       effective_make_and_model.empty() ? PpdProvider::NOT_FOUND
                                                        : PpdProvider::SUCCESS,
                       effective_make_and_model.as_string()));
  }

  // These methods are not used by CupsPrintersManager.
  void ResolvePpd(const Printer::PpdReference& reference,
                  ResolvePpdCallback cb) override {}
  void ResolvePpdReference(const chromeos::PrinterSearchData& search_data,
                           ResolvePpdReferenceCallback cb) override {}
  void ResolveManufacturers(ResolveManufacturersCallback cb) override {}
  void ResolvePrinters(const std::string& manufacturer,
                       ResolvePrintersCallback cb) override {}
  void ReverseLookup(const std::string& effective_make_and_model,
                     ReverseLookupCallback cb) override {}

 private:
  ~FakePpdProvider() override = default;
};

}  // namespace

class LocalPrinterHandlerChromeosTest : public testing::Test {
 public:
  LocalPrinterHandlerChromeosTest() = default;
  ~LocalPrinterHandlerChromeosTest() override = default;

  void SetUp() override {
    test_backend_ = base::MakeRefCounted<TestPrintBackend>();
    PrintBackend::SetPrintBackendForTesting(test_backend_.get());
    ppd_provider_ = base::MakeRefCounted<FakePpdProvider>();
    local_printer_handler_ = LocalPrinterHandlerChromeos::CreateForTesting(
        &profile_, nullptr, &printers_manager_,
        std::make_unique<chromeos::TestPrinterConfigurer>(), ppd_provider_);
  }

 protected:
  // Must outlive |profile_|.
  content::BrowserTaskEnvironment task_environment_;
  // Must outlive |printers_manager_|.
  TestingProfile profile_;
  scoped_refptr<TestPrintBackend> test_backend_;
  chromeos::TestCupsPrintersManager printers_manager_;
  scoped_refptr<FakePpdProvider> ppd_provider_;
  std::unique_ptr<LocalPrinterHandlerChromeos> local_printer_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalPrinterHandlerChromeosTest);
};

TEST_F(LocalPrinterHandlerChromeosTest, GetPrinters) {
  size_t call_count = 0;
  std::unique_ptr<base::ListValue> printers;
  bool is_done = false;

  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  Printer enterprise_printer =
      CreateEnterprisePrinter("printer2", "enterprise", "description2");
  Printer automatic_printer =
      CreateTestPrinter("printer3", "automatic", "description3");

  printers_manager_.AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager_.AddPrinter(enterprise_printer, PrinterClass::kEnterprise);
  printers_manager_.AddPrinter(automatic_printer, PrinterClass::kAutomatic);

  local_printer_handler_->StartGetPrinters(
      base::BindRepeating(&RecordPrinterList, &call_count, &printers),
      base::BindOnce(&RecordPrintersDone, &is_done));

  EXPECT_EQ(call_count, 1u);
  EXPECT_TRUE(is_done);
  ASSERT_TRUE(printers);

  const std::string expected_list = R"(
    [
      {
        "cupsEnterprisePrinter": false,
        "deviceName": "printer1",
        "printerDescription": "description1",
        "printerName": "saved",
        "printerOptions": {
          "cupsEnterprisePrinter": "false"
        }
      },
      {
        "cupsEnterprisePrinter": true,
        "deviceName": "printer2",
        "printerDescription": "description2",
        "printerName": "enterprise",
        "printerOptions": {
          "cupsEnterprisePrinter": "true"
        }
      },
      {
        "cupsEnterprisePrinter": false,
        "deviceName": "printer3",
        "printerDescription": "description3",
        "printerName": "automatic",
        "printerOptions": {
          "cupsEnterprisePrinter": "false"
        }
      }
    ]
  )";
  std::string error;
  std::unique_ptr<base::ListValue> expected_printers(
      GetJSONAsListValue(expected_list, &error));
  ASSERT_TRUE(expected_printers) << "Error deserializing printers: " << error;
  EXPECT_EQ(*printers, *expected_printers);
}

// Tests that fetching capabilities for an existing installed printer is
// successful.
TEST_F(LocalPrinterHandlerChromeosTest, StartGetCapabilityValidPrinter) {
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager_.AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager_.InstallPrinter("printer1");

  // Add printer capabilities to |test_backend_|.
  PrinterSemanticCapsAndDefaults caps;
  test_backend_->AddValidPrinter(
      "printer1", std::make_unique<PrinterSemanticCapsAndDefaults>(caps),
      std::make_unique<printing::PrinterBasicInfo>());

  std::unique_ptr<base::Value> fetched_caps;
  local_printer_handler_->StartGetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, &fetched_caps));

  task_environment_.RunUntilIdle();

  ASSERT_TRUE(fetched_caps);
  base::DictionaryValue* dict;
  ASSERT_TRUE(fetched_caps->GetAsDictionary(&dict));
  ASSERT_TRUE(dict->HasKey(kSettingCapabilities));
  ASSERT_TRUE(dict->HasKey(kPrinter));
}

// Test that printers which have not yet been installed are installed with
// SetUpPrinter before their capabilities are fetched.
TEST_F(LocalPrinterHandlerChromeosTest, StartGetCapabilityPrinterNotInstalled) {
  Printer discovered_printer =
      CreateTestPrinter("printer1", "discovered", "description1");
  // NOTE: The printer |discovered_printer| is not installed using
  // InstallPrinter.
  printers_manager_.AddPrinter(discovered_printer, PrinterClass::kDiscovered);

  // Add printer capabilities to |test_backend_|.
  PrinterSemanticCapsAndDefaults caps;
  test_backend_->AddValidPrinter(
      "printer1", std::make_unique<PrinterSemanticCapsAndDefaults>(caps),
      std::make_unique<printing::PrinterBasicInfo>());

  std::unique_ptr<base::Value> fetched_caps;
  local_printer_handler_->StartGetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, &fetched_caps));

  task_environment_.RunUntilIdle();

  ASSERT_TRUE(fetched_caps);
  base::DictionaryValue* dict;
  ASSERT_TRUE(fetched_caps->GetAsDictionary(&dict));
  ASSERT_TRUE(dict->HasKey(kSettingCapabilities));
  ASSERT_TRUE(dict->HasKey(kPrinter));
}

// In this test we expect the StartGetCapability to bail early because the
// provided printer can't be found in the CupsPrintersManager.
TEST_F(LocalPrinterHandlerChromeosTest, StartGetCapabilityInvalidPrinter) {
  std::unique_ptr<base::Value> fetched_caps;
  local_printer_handler_->StartGetCapability(
      "invalid printer", base::BindOnce(&RecordGetCapability, &fetched_caps));

  task_environment_.RunUntilIdle();

  ASSERT_TRUE(fetched_caps);
  EXPECT_TRUE(fetched_caps->is_none());
}

TEST_F(LocalPrinterHandlerChromeosTest, GetNativePrinterPolicies) {
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile_.GetTestingPrefService();

  prefs->SetUserPref(prefs::kPrintingAllowedColorModes,
                     std::make_unique<base::Value>(1));
  prefs->SetUserPref(prefs::kPrintingAllowedDuplexModes,
                     std::make_unique<base::Value>(0));
  prefs->SetUserPref(prefs::kPrintingAllowedPinModes,
                     std::make_unique<base::Value>(1));
  prefs->SetUserPref(prefs::kPrintingColorDefault,
                     std::make_unique<base::Value>(2));
  prefs->SetUserPref(prefs::kPrintingDuplexDefault,
                     std::make_unique<base::Value>(4));
  prefs->SetUserPref(prefs::kPrintingPinDefault,
                     std::make_unique<base::Value>(0));

  base::Value expected_policies(base::Value::Type::DICTIONARY);
  expected_policies.SetKey(kAllowedColorModes, base::Value(1));
  expected_policies.SetKey(kAllowedDuplexModes, base::Value(0));
  expected_policies.SetKey(kAllowedPinModes, base::Value(1));
  expected_policies.SetKey(kDefaultColorMode, base::Value(2));
  expected_policies.SetKey(kDefaultDuplexMode, base::Value(4));
  expected_policies.SetKey(kDefaultPinMode, base::Value(0));

  EXPECT_EQ(expected_policies,
            local_printer_handler_->GetNativePrinterPolicies());
}

// Test that fetching a PPD license will return a license if the printer has one
// available.
TEST_F(LocalPrinterHandlerChromeosTest, StartFetchValidEulaUrl) {
  Printer::PpdReference ref;
  ref.effective_make_and_model = "expected_make_model";

  // Printers with a PpdReference will return a license
  Printer saved_printer = CreateTestPrinterWithPpdReference(
      "printer1", "saved", "description1", ref);
  printers_manager_.AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager_.InstallPrinter("printer1");

  std::string fetched_eula_url;
  local_printer_handler_->StartGetEulaUrl(
      "printer1", base::BindOnce(&RecordGetEulaUrl, &fetched_eula_url));

  task_environment_.RunUntilIdle();

  EXPECT_EQ(fetched_eula_url, "chrome://os-credits/#expected_make_model");
}

// Test that a printer with no PPD license will return an empty string.
TEST_F(LocalPrinterHandlerChromeosTest, StartFetchNotFoundEulaUrl) {
  // A printer without a PpdReference will simulate an PPD without a license.
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager_.AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager_.InstallPrinter("printer1");

  std::string fetched_eula_url;
  local_printer_handler_->StartGetEulaUrl(
      "printer1", base::BindOnce(&RecordGetEulaUrl, &fetched_eula_url));

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(fetched_eula_url.empty());
}

// Test that fetching a PPD license will exit early if the printer is not found
// in CupsPrintersManager.
TEST_F(LocalPrinterHandlerChromeosTest, StartFetchEulaUrlOnNonExistantPrinter) {
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");

  std::string fetched_eula_url;
  local_printer_handler_->StartGetEulaUrl(
      "printer1", base::BindOnce(&RecordGetEulaUrl, &fetched_eula_url));

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(fetched_eula_url.empty());
}

}  // namespace printing
