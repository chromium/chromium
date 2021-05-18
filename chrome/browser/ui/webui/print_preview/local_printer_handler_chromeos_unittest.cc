// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_chromeos.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/chromeos/printing/test_cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/test_printer_configurer.h"
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/printing/print_backend_service_test_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/printing/printer_capabilities.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/printing/ppd_provider.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/printing_restrictions.h"
#include "printing/backend/test_print_backend.h"
#include "printing/print_job_constants.h"
#include "printing/printing_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

using chromeos::CupsPrintersManager;
using chromeos::Printer;
using chromeos::PrinterClass;
using chromeos::PrinterConfigurer;
using chromeos::PrinterSetupCallback;
using chromeos::PrinterSetupResult;

// Used as a callback to `StartGetPrinters()` in tests.
// Increases `call_count` and records values returned by `StartGetPrinters()`.
// TODO(crbug.com/1171579) Get rid of use of base::ListValue.
void RecordPrinterList(size_t& call_count,
                       std::unique_ptr<base::ListValue>& printers_out,
                       const base::ListValue& printers) {
  ++call_count;
  printers_out.reset(printers.DeepCopy());
}

// Used as a callback to `StartGetPrinters` in tests.
// Records that the test is done.
void RecordPrintersDone(bool& is_done_out) {
  is_done_out = true;
}

void RecordGetCapability(base::Value& capabilities_out,
                         base::Value capability) {
  capabilities_out = std::move(capability);
}

void RecordGetEulaUrl(std::string& fetched_eula_url,
                      const std::string& eula_url) {
  fetched_eula_url = eula_url;
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

// Converts JSON string to `base::Value` object.
// On failure, fills `error` string and the return value is not a list.
base::Value GetJSONAsValue(const std::string& json, std::string& error) {
  return base::Value::FromUniquePtrValue(
      JSONStringValueDeserializer(json).Deserialize(nullptr, &error));
}

// Fake `PpdProvider` backend. This fake `PpdProvider` is used to fake fetching
// the PPD EULA license of a destination. If `effective_make_and_model` is
// empty, it will return with NOT_FOUND and an empty string. Otherwise, it will
// return SUCCESS with `effective_make_and_model` as the PPD license.
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
                       std::string(effective_make_and_model)));
  }

  // These methods are not used by `CupsPrintersManager`.
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

// Base testing class for `LocalPrinterHandlerChromeos`.  Contains the base
// logic to allow for using either a local task runner or a service to make
// print backend calls, and to possibly enable fallback when using a service.
// Tests to trigger those different paths can be done by overloading
// `UseService()` and `SupportFallback()`.
class LocalPrinterHandlerChromeosTestBase : public testing::Test {
 public:
  LocalPrinterHandlerChromeosTestBase() = default;
  LocalPrinterHandlerChromeosTestBase(
      const LocalPrinterHandlerChromeosTestBase&) = delete;
  LocalPrinterHandlerChromeosTestBase& operator=(
      const LocalPrinterHandlerChromeosTestBase&) = delete;
  ~LocalPrinterHandlerChromeosTestBase() override = default;

  TestPrintBackend* sandboxed_print_backend() {
    return sandboxed_test_backend_.get();
  }
  TestPrintBackend* unsandboxed_print_backend() {
    return unsandboxed_test_backend_.get();
  }

  // Indicate if calls to print backend should be made using a service instead
  // of a local task runner.
  virtual bool UseService() = 0;

  // Indicate if fallback support for access-denied errors should be included
  // when using a service for print backend calls.
  virtual bool SupportFallback() = 0;

  void SetUp() override {
    // Choose between running with local test runner or via a service.
    feature_list_.InitWithFeatureState(features::kEnableOopPrintDrivers,
                                       UseService());

    sandboxed_test_backend_ = base::MakeRefCounted<TestPrintBackend>();
    ppd_provider_ = base::MakeRefCounted<FakePpdProvider>();
    local_printer_handler_ = LocalPrinterHandlerChromeos::CreateForTesting(
        &profile_, nullptr, &printers_manager_,
        std::make_unique<chromeos::TestPrinterConfigurer>(), ppd_provider_);

    if (UseService()) {
      sandboxed_print_backend_service_ =
          PrintBackendServiceTestImpl::LaunchForTesting(sandboxed_test_remote_,
                                                        sandboxed_test_backend_,
                                                        /*sandboxed=*/true);

      if (SupportFallback()) {
        unsandboxed_test_backend_ = base::MakeRefCounted<TestPrintBackend>();

        unsandboxed_print_backend_service_ =
            PrintBackendServiceTestImpl::LaunchForTesting(
                unsandboxed_test_remote_, unsandboxed_test_backend_,
                /*sandboxed=*/false);
      }
    } else {
      // Use of task runners will call `PrintBackend::CreateInstance()`, which
      // needs a test backend registered for it to use.
      PrintBackend::SetPrintBackendForTesting(sandboxed_test_backend_.get());
    }
  }

  void TearDown() override { PrintBackendServiceManager::ResetForTesting(); }

 protected:
  void AddPrinter(const std::string& id,
                  const std::string& display_name,
                  const std::string& description,
                  bool is_default,
                  bool requires_elevated_permissions) {
    auto caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
    caps->papers.push_back({"bar", "vendor", {600, 600}});
    auto basic_info = std::make_unique<PrinterBasicInfo>(
        id, display_name, description, /*printer_status=*/0, is_default,
        PrinterBasicInfoOptions{});

    if (SupportFallback()) {
      // Need to populate same values into a second print backend.
      // For fallback they will always be treated as valid.
      auto caps_unsandboxed =
          std::make_unique<PrinterSemanticCapsAndDefaults>(*caps);
      auto basic_info_unsandboxed =
          std::make_unique<PrinterBasicInfo>(*basic_info);
      unsandboxed_print_backend()->AddValidPrinter(
          id, std::move(caps_unsandboxed), std::move(basic_info_unsandboxed));
    }

    if (requires_elevated_permissions) {
      sandboxed_print_backend()->AddAccessDeniedPrinter(id);
    } else {
      sandboxed_print_backend()->AddValidPrinter(id, std::move(caps),
                                                 std::move(basic_info));
    }
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  TestingProfile& profile() { return profile_; }

  chromeos::TestCupsPrintersManager& printers_manager() {
    return printers_manager_;
  }

  LocalPrinterHandlerChromeos* local_printer_handler() {
    return local_printer_handler_.get();
  }

 private:
  // Must outlive `profile_`.
  content::BrowserTaskEnvironment task_environment_;
  // Must outlive `printers_manager_`.
  TestingProfile profile_;
  scoped_refptr<TestPrintBackend> sandboxed_test_backend_;
  scoped_refptr<TestPrintBackend> unsandboxed_test_backend_;
  chromeos::TestCupsPrintersManager printers_manager_;
  scoped_refptr<FakePpdProvider> ppd_provider_;
  std::unique_ptr<LocalPrinterHandlerChromeos> local_printer_handler_;

  // Support for testing via a service instead of with a local task runner.
  base::test::ScopedFeatureList feature_list_;
  mojo::Remote<mojom::PrintBackendService> sandboxed_test_remote_;
  mojo::Remote<mojom::PrintBackendService> unsandboxed_test_remote_;
  std::unique_ptr<PrintBackendServiceTestImpl> sandboxed_print_backend_service_;
  std::unique_ptr<PrintBackendServiceTestImpl>
      unsandboxed_print_backend_service_;
};

// Testing class to cover `LocalPrinterHandlerChromeos` handling using a local
// task runner.
class LocalPrinterHandlerChromeosTest
    : public LocalPrinterHandlerChromeosTestBase {
 public:
  LocalPrinterHandlerChromeosTest() = default;
  LocalPrinterHandlerChromeosTest(const LocalPrinterHandlerChromeosTest&) =
      delete;
  LocalPrinterHandlerChromeosTest& operator=(
      const LocalPrinterHandlerChromeosTest&) = delete;
  ~LocalPrinterHandlerChromeosTest() override = default;

  bool UseService() override { return false; }
  bool SupportFallback() override { return false; }
};

// Testing class to cover `LocalPrinterHandlerChromeos` handling using either a
// local task runner or a service.  Makes no attempt to cover fallback when
// using a service, which is handled separately by
// `LocalPrinterHandlerChromeosFallbackTest`
class LocalPrinterHandlerChromeosProcessScopeTest
    : public LocalPrinterHandlerChromeosTestBase,
      public testing::WithParamInterface<bool> {
 public:
  LocalPrinterHandlerChromeosProcessScopeTest() = default;
  LocalPrinterHandlerChromeosProcessScopeTest(
      const LocalPrinterHandlerChromeosProcessScopeTest&) = delete;
  LocalPrinterHandlerChromeosProcessScopeTest& operator=(
      const LocalPrinterHandlerChromeosProcessScopeTest&) = delete;
  ~LocalPrinterHandlerChromeosProcessScopeTest() override = default;

  bool UseService() override { return GetParam(); }
  bool SupportFallback() override { return false; }
};

// Testing class to cover `LocalPrinterHandlerChromeos` handling using only a
// service and when fallback could yield different results.
class LocalPrinterHandlerChromeosFallbackTest
    : public LocalPrinterHandlerChromeosTestBase {
 public:
  LocalPrinterHandlerChromeosFallbackTest() = default;
  LocalPrinterHandlerChromeosFallbackTest(
      const LocalPrinterHandlerChromeosFallbackTest&) = delete;
  LocalPrinterHandlerChromeosFallbackTest& operator=(
      const LocalPrinterHandlerChromeosFallbackTest&) = delete;
  ~LocalPrinterHandlerChromeosFallbackTest() override = default;

  bool UseService() override { return true; }
  bool SupportFallback() override { return true; }
};

INSTANTIATE_TEST_SUITE_P(All,
                         LocalPrinterHandlerChromeosProcessScopeTest,
                         testing::Bool());

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

  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().AddPrinter(enterprise_printer, PrinterClass::kEnterprise);
  printers_manager().AddPrinter(automatic_printer, PrinterClass::kAutomatic);

  local_printer_handler()->StartGetPrinters(
      base::BindRepeating(&RecordPrinterList, std::ref(call_count),
                          std::ref(printers)),
      base::BindOnce(&RecordPrintersDone, std::ref(is_done)));

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
  base::Value expected_printers(GetJSONAsValue(expected_list, error));
  ASSERT_TRUE(expected_printers.is_list())
      << "Error deserializing printers: " << error;
  EXPECT_EQ(*printers, expected_printers);
}

// Tests that fetching capabilities for an existing installed printer is
// successful.
TEST_P(LocalPrinterHandlerChromeosProcessScopeTest,
       StartGetCapabilityValidPrinter) {
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().InstallPrinter("printer1");

  // Add printer capabilities to `test_backend_`.
  AddPrinter("printer1", "saved", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);

  base::Value fetched_caps("dummy");
  local_printer_handler()->StartGetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_caps.FindDictKey(kSettingCapabilities));
  EXPECT_TRUE(fetched_caps.FindDictKey(kPrinter));
}

// Test that printers which have not yet been installed are installed with
// `SetUpPrinter` before their capabilities are fetched.
TEST_P(LocalPrinterHandlerChromeosProcessScopeTest,
       StartGetCapabilityPrinterNotInstalled) {
  Printer discovered_printer =
      CreateTestPrinter("printer1", "discovered", "description1");
  // NOTE: The printer `discovered_printer` is not installed using
  // `InstallPrinter`.
  printers_manager().AddPrinter(discovered_printer, PrinterClass::kDiscovered);

  // Add printer capabilities to `test_backend_`.
  AddPrinter("printer1", "discovered", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);

  base::Value fetched_caps("dummy");
  local_printer_handler()->StartGetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_caps.FindDictKey(kSettingCapabilities));
  EXPECT_TRUE(fetched_caps.FindDictKey(kPrinter));
}

// In this test we expect the `StartGetCapability` to bail early because the
// provided printer can't be found in the `CupsPrintersManager`.
TEST_P(LocalPrinterHandlerChromeosProcessScopeTest,
       StartGetCapabilityInvalidPrinter) {
  base::Value fetched_caps("dummy");
  local_printer_handler()->StartGetCapability(
      "invalid printer",
      base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_caps.is_none());
}

// Test that installed printers to which the user does not have permission to
// access will receive a dictionary for the capabilities but will not have any
// settings in that.
TEST_P(LocalPrinterHandlerChromeosProcessScopeTest,
       StartGetCapabilityAccessDenied) {
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().InstallPrinter("printer1");

  // Add printer capabilities to `test_backend_`.
  AddPrinter("printer1", "saved", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/true);

  base::Value fetched_caps("dummy");
  local_printer_handler()->StartGetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  const base::Value* settings = fetched_caps.FindDictKey(kSettingCapabilities);
  ASSERT_TRUE(settings);
  ASSERT_TRUE(settings->is_dict());
  EXPECT_TRUE(settings->DictEmpty());
}

TEST_F(LocalPrinterHandlerChromeosFallbackTest,
       StartGetCapabilityElevatedPermissionsSucceeds) {
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().InstallPrinter("printer1");

  // Add printer capabilities to `test_backend_`.
  AddPrinter("printer1", "saved", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/true);

  // Note that printer does not initially show as requiring elevated privileges.
  EXPECT_FALSE(PrintBackendServiceManager::GetInstance()
                   .PrinterDriverRequiresElevatedPrivilege("printer1"));

  base::Value fetched_caps("dummy");
  local_printer_handler()->StartGetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  // Getting capabilities should succeed when fallback is supported.
  const base::Value* settings = fetched_caps.FindDictKey(kSettingCapabilities);
  ASSERT_TRUE(settings);
  EXPECT_TRUE(settings->FindDictKey(kPrinter));

  // Verify that this printer now shows up as requiring elevated privileges.
  EXPECT_TRUE(PrintBackendServiceManager::GetInstance()
                  .PrinterDriverRequiresElevatedPrivilege("printer1"));
}

TEST_F(LocalPrinterHandlerChromeosTest, GetNativePrinterPolicies) {
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile().GetTestingPrefService();

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
            local_printer_handler()->GetNativePrinterPolicies());
}

// Test that fetching a PPD license will return a license if the printer has one
// available.
TEST_F(LocalPrinterHandlerChromeosTest, StartFetchValidEulaUrl) {
  Printer::PpdReference ref;
  ref.effective_make_and_model = "expected_make_model";

  // Printers with a `PpdReference` will return a license
  Printer saved_printer = CreateTestPrinterWithPpdReference(
      "printer1", "saved", "description1", ref);
  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().InstallPrinter("printer1");

  std::string fetched_eula_url;
  local_printer_handler()->StartGetEulaUrl(
      "printer1",
      base::BindOnce(&RecordGetEulaUrl, std::ref(fetched_eula_url)));

  RunUntilIdle();

  EXPECT_EQ(fetched_eula_url, "chrome://os-credits/#expected_make_model");
}

// Test that a printer with no PPD license will return an empty string.
TEST_F(LocalPrinterHandlerChromeosTest, StartFetchNotFoundEulaUrl) {
  // A printer without a `PpdReference` will simulate an PPD without a license.
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");
  printers_manager().AddPrinter(saved_printer, PrinterClass::kSaved);
  printers_manager().InstallPrinter("printer1");

  std::string fetched_eula_url;
  local_printer_handler()->StartGetEulaUrl(
      "printer1",
      base::BindOnce(&RecordGetEulaUrl, std::ref(fetched_eula_url)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_eula_url.empty());
}

// Test that fetching a PPD license will exit early if the printer is not found
// in `CupsPrintersManager`.
TEST_F(LocalPrinterHandlerChromeosTest, StartFetchEulaUrlOnNonExistantPrinter) {
  Printer saved_printer =
      CreateTestPrinter("printer1", "saved", "description1");

  std::string fetched_eula_url;
  local_printer_handler()->StartGetEulaUrl(
      "printer1",
      base::BindOnce(&RecordGetEulaUrl, std::ref(fetched_eula_url)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_eula_url.empty());
}

}  // namespace printing
