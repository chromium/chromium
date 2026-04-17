// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_chromeos.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/fake_local_printer.h"
#include "chrome/browser/ash/printing/ipp_client_info_calculator.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/chromeos/printing/fake_local_printer_chromeos.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/test/test_user_session_manager.h"
#include "components/user_manager/test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "printing/backend/print_backend.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings_conversion_chromeos.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

using ::printing::mojom::IppClientInfo;
using ::printing::mojom::IppClientInfoPtr;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::WithArg;

constexpr auto kStatusTimestamp = base::Time::FromSecondsSinceUnixEpoch(1e9);
constexpr char kUsername[] = "chronos";
constexpr char kEmail[] = "email@example.com";
constexpr auto kAccountId =
    AccountId::Literal::FromUserEmailGaiaId(kEmail,
                                            GaiaId::Literal("123456789"));

class FakeIppClientInfoCalculator
    : public ash::printing::IppClientInfoCalculator {
 public:
  FakeIppClientInfoCalculator() = default;
  ~FakeIppClientInfoCalculator() override = default;

  printing::mojom::IppClientInfoPtr GetOsInfo() const override {
    get_os_info_called_ = true;
    return os_info_ ? os_info_->Clone() : nullptr;
  }

  printing::mojom::IppClientInfoPtr GetDeviceInfo() const override {
    get_device_info_called_ = true;
    return device_info_ ? device_info_->Clone() : nullptr;
  }

  void SetOsInfo(printing::mojom::IppClientInfoPtr os_info) {
    os_info_ = std::move(os_info);
  }

  void SetDeviceInfo(printing::mojom::IppClientInfoPtr device_info) {
    device_info_ = std::move(device_info);
  }

  bool get_os_info_called() const { return get_os_info_called_; }
  bool get_device_info_called() const { return get_device_info_called_; }

 private:
  printing::mojom::IppClientInfoPtr os_info_;
  printing::mojom::IppClientInfoPtr device_info_;
  mutable bool get_os_info_called_ = false;
  mutable bool get_device_info_called_ = false;
};

// Used as a callback to `StartGetPrinters()` in tests.
// Increases `call_count` and records values returned by `StartGetPrinters()`.
void RecordPrinterList(size_t& call_count,
                       base::ListValue& printers_out,
                       base::ListValue printers) {
  ++call_count;
  printers_out = std::move(printers);
}

// Used as a callback to `StartGetPrinters` in tests.
// Records that the test is done.
void RecordPrintersDone(bool& is_done_out) {
  is_done_out = true;
}

void RecordGetCapability(base::DictValue& capabilities_out,
                         base::DictValue capability) {
  capabilities_out = std::move(capability);
}

void RecordGetEulaUrl(std::string& fetched_eula_url,
                      const std::string& eula_url) {
  fetched_eula_url = eula_url;
}

void RecordAshJobSettings(base::DictValue& fetched_settings,
                          base::DictValue settings) {
  fetched_settings = std::move(settings);
}

const base::DictValue kInitialJobSettings = base::test::ParseJsonDict(R"({
  "key": "value"
})");

}  // namespace

// Test that the printer handler runs callbacks with reasonable defaults when
// the mojo connection to ash cannot be established, which should never occur in
// production but may occur in unit/browser tests.
class LocalPrinterHandlerChromeosNoAshTest : public testing::Test {
 public:
  LocalPrinterHandlerChromeosNoAshTest() = default;
  LocalPrinterHandlerChromeosNoAshTest(
      const LocalPrinterHandlerChromeosNoAshTest&) = delete;
  LocalPrinterHandlerChromeosNoAshTest& operator=(
      const LocalPrinterHandlerChromeosNoAshTest&) = delete;
  ~LocalPrinterHandlerChromeosNoAshTest() override = default;

  void SetUp() override {
    local_printer_handler_ = LocalPrinterHandlerChromeos::CreateForTesting(
        /*local_printer=*/nullptr,
        /*ipp_client_info_calculator=*/nullptr);
  }

  LocalPrinterHandlerChromeos* local_printer_handler() {
    return local_printer_handler_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<LocalPrinterHandlerChromeos> local_printer_handler_;
};

// Test that the printer handler runs callbacks with the correct values received
// from a mocked mojo connection to ash.
class LocalPrinterHandlerChromeosWithAshTest : public testing::Test {
 public:
  LocalPrinterHandlerChromeosWithAshTest() = default;
  LocalPrinterHandlerChromeosWithAshTest(
      const LocalPrinterHandlerChromeosWithAshTest&) = delete;
  LocalPrinterHandlerChromeosWithAshTest& operator=(
      const LocalPrinterHandlerChromeosWithAshTest&) = delete;
  ~LocalPrinterHandlerChromeosWithAshTest() override = default;

  void SetUp() override {
    test_user_session_manager_ =
        std::make_unique<ash::test::TestUserSessionManager>(
            TestingBrowserProcess::GetGlobal()->GetTestingLocalState());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    ASSERT_TRUE(test_user_session_manager_->AddRegularUser(kAccountId));
    test_user_session_manager_->LogIn(kAccountId);

    profile_ = profile_manager_->CreateTestingProfile(kEmail);
    ash::AnnotatedAccountId::Set(profile_, kAccountId);
    user_manager::UserManager::Get()->OnUserProfileCreated(
        kAccountId, profile_->GetPrefs());

    auto ipp_client_info_calculator =
        std::make_unique<FakeIppClientInfoCalculator>();
    ipp_client_info_calculator_ = ipp_client_info_calculator.get();
    local_printer_handler_ = LocalPrinterHandlerChromeos::CreateForTesting(
        &local_printer_, std::move(ipp_client_info_calculator));
  }

  void TearDown() override {
    ipp_client_info_calculator_ = nullptr;
    local_printer_handler_.reset();
    user_manager::UserManager::Get()->OnUserProfileWillBeDestroyed(kAccountId);
    profile_ = nullptr;
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
    test_user_session_manager_.reset();
  }

  LocalPrinterHandlerChromeos* local_printer_handler() {
    return local_printer_handler_.get();
  }
  ash::FakeLocalPrinter& local_printer() { return local_printer_; }
  FakeIppClientInfoCalculator& ipp_client_info_calculator() {
    return *ipp_client_info_calculator_;
  }
  ash::test::TestUserSessionManager* test_user_session_manager() {
    return test_user_session_manager_.get();
  }

  TestingProfile* profile() { return profile_; }

 private:
  ash::FakeLocalPrinter local_printer_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ash::test::TestUserSessionManager> test_user_session_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<LocalPrinterHandlerChromeos> local_printer_handler_;
  raw_ptr<TestingProfile> profile_;
  raw_ptr<FakeIppClientInfoCalculator> ipp_client_info_calculator_;
};

TEST_F(LocalPrinterHandlerChromeosNoAshTest,
       PrinterStatusRequest_ProvidesDefaultValue) {
  std::optional<base::DictValue> printer_status = base::DictValue();
  local_printer_handler()->StartPrinterStatusRequest(
      "printer1",
      base::BindLambdaForTesting([&](std::optional<base::DictValue> status) {
        printer_status = std::move(status);
      }));
  EXPECT_EQ(std::nullopt, printer_status);
}

TEST_F(LocalPrinterHandlerChromeosNoAshTest, GetPrinters_ProvidesDefaultValue) {
  size_t call_count = 0;
  base::ListValue printers;
  bool is_done = false;
  local_printer_handler()->StartGetPrinters(
      base::BindRepeating(&RecordPrinterList, std::ref(call_count),
                          std::ref(printers)),
      base::BindOnce(&RecordPrintersDone, std::ref(is_done)));
  // RecordPrinterList is called when printers are discovered. If no printers
  // are discovered, the function is not called.
  EXPECT_EQ(0u, call_count);
  // RecordPrintersDone is called once printer discovery is finished, even if no
  // printers have been discovered.
  EXPECT_TRUE(is_done);
}

TEST_F(LocalPrinterHandlerChromeosNoAshTest,
       GetDefaultPrinter_ProvidesDefaultValue) {
  std::string default_printer = "unset";
  local_printer_handler()->GetDefaultPrinter(base::BindLambdaForTesting(
      [&](const std::string& printer) { default_printer = printer; }));
  EXPECT_EQ("", default_printer);
}

TEST_F(LocalPrinterHandlerChromeosNoAshTest,
       GetCapability_ProvidesDefaultValue) {
  base::DictValue fetched_caps;
  local_printer_handler()->StartGetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));
  EXPECT_TRUE(fetched_caps.empty());
}

TEST_F(LocalPrinterHandlerChromeosNoAshTest, GetEulaUrl_ProvidesDefaultValue) {
  std::string fetched_eula_url = "unset";
  local_printer_handler()->StartGetEulaUrl(
      "printer1",
      base::BindOnce(&RecordGetEulaUrl, std::ref(fetched_eula_url)));
  EXPECT_EQ("", fetched_eula_url);
}

TEST_F(LocalPrinterHandlerChromeosNoAshTest, GetAshJobSettingsEmpty) {
  base::DictValue fetched_settings;
  local_printer_handler()->GetAshJobSettingsForTesting(
      "printer1",
      base::BindOnce(&RecordAshJobSettings, std::ref(fetched_settings)),
      kInitialJobSettings.Clone());

  EXPECT_EQ(fetched_settings, kInitialJobSettings);
}

TEST_F(LocalPrinterHandlerChromeosWithAshTest, GetAshJobSettingsEmpty) {
  local_printer().AddPrinter(chromeos::Printer("printer1"));
  base::DictValue fetched_settings;
  local_printer_handler()->GetAshJobSettingsForTesting(
      "printer1",
      base::BindOnce(&RecordAshJobSettings, std::ref(fetched_settings)),
      kInitialJobSettings.Clone());

  EXPECT_EQ(fetched_settings, kInitialJobSettings);
  EXPECT_EQ(1, local_printer().get_printer_call_count());
  EXPECT_TRUE(ipp_client_info_calculator().get_os_info_called());
  EXPECT_FALSE(ipp_client_info_calculator().get_device_info_called());
}

TEST_F(LocalPrinterHandlerChromeosWithAshTest, GetAshJobSettingsUsername) {
  local_printer().AddPrinter(chromeos::Printer("printer1"));
  user_manager::UserManager::Get()->SaveUserDisplayEmail(kAccountId, kUsername);
  profile()->GetPrefs()->SetBoolean(
      ash::prefs::kPrintingSendUsernameAndFilenameEnabled, true);

  base::DictValue fetched_settings;
  local_printer_handler()->GetAshJobSettingsForTesting(
      "printer1",
      base::BindOnce(&RecordAshJobSettings, std::ref(fetched_settings)),
      kInitialJobSettings.Clone());

  // Test that `username` and `sendUserInfo` are in job settings, together with
  // the old settings.
  const base::DictValue kExpectedValue = base::test::ParseJsonDict(R"({
    "key": "value",
    "username": "chronos",
    "sendUserInfo": true
  })");
  EXPECT_EQ(fetched_settings, kExpectedValue);
  EXPECT_EQ(1, local_printer().get_printer_call_count());
  EXPECT_TRUE(ipp_client_info_calculator().get_os_info_called());
  EXPECT_FALSE(ipp_client_info_calculator().get_device_info_called());
}

TEST_F(LocalPrinterHandlerChromeosWithAshTest, GetEulaUrl) {
  local_printer().AddPrinter(chromeos::Printer("printer1"));
  local_printer().SetEulaUrl("printer1", GURL("https://example.com/eula"));
  std::string fetched_eula_url = "unset";
  local_printer_handler()->StartGetEulaUrl(
      "printer1",
      base::BindOnce(&RecordGetEulaUrl, std::ref(fetched_eula_url)));
  EXPECT_EQ("https://example.com/eula", fetched_eula_url);
  EXPECT_EQ(1, local_printer().get_eula_url_call_count());
}

TEST_F(LocalPrinterHandlerChromeosWithAshTest, GetAshJobSettingsOAuthToken) {
  local_printer().AddPrinter(chromeos::Printer("printer1"));
  local_printer().SetOAuthAccessToken("printer1", "token");

  base::DictValue fetched_settings;
  local_printer_handler()->GetAshJobSettingsForTesting(
      "printer1",
      base::BindOnce(&RecordAshJobSettings, std::ref(fetched_settings)),
      kInitialJobSettings.Clone());

  // Test that oauth token is in job settings, together with the old settings.
  const base::DictValue kExpectedValue = base::test::ParseJsonDict(R"({
    "key": "value",
    "chromeos-access-oauth-token": "token"
  })");
  EXPECT_EQ(fetched_settings, kExpectedValue);
  EXPECT_EQ(1, local_printer().get_oauth_token_call_count());
}

TEST_F(LocalPrinterHandlerChromeosWithAshTest,
       GetAshJobSettingsClientInfoEmptyPrinterId) {
  base::DictValue fetched_settings;
  local_printer_handler()->GetAshJobSettingsForTesting(
      "", base::BindOnce(&RecordAshJobSettings, std::ref(fetched_settings)),
      kInitialJobSettings.Clone());

  EXPECT_EQ(fetched_settings, kInitialJobSettings);
  EXPECT_EQ(0, local_printer().get_printer_call_count());
  EXPECT_FALSE(ipp_client_info_calculator().get_os_info_called());
  EXPECT_FALSE(ipp_client_info_calculator().get_device_info_called());
}

TEST_F(LocalPrinterHandlerChromeosWithAshTest, GetAshJobSettingsClientInfo) {
  user_manager::UserManager::Get()->SetUserPolicyStatus(
      kAccountId, /*is_managed=*/true, /*is_affiliated=*/true);
  chromeos::Printer printer("printer1");
  printer.set_source(chromeos::Printer::Source::SRC_POLICY);
  printer.SetUri("ipps://printer.example.com/");
  local_printer().AddPrinter(printer);
  const std::vector<IppClientInfo> expected_client_info{
      {IppClientInfo::ClientType::kOperatingSystem, "ChromeOS", "patch",
       "str_version", "version"},
      {IppClientInfo::ClientType::kOther, "chromebook-42", std::nullopt, "",
       std::nullopt}};
  ipp_client_info_calculator().SetOsInfo(expected_client_info[0].Clone());
  ipp_client_info_calculator().SetDeviceInfo(expected_client_info[1].Clone());

  base::DictValue fetched_settings;
  local_printer_handler()->GetAshJobSettingsForTesting(
      "printer1",
      base::BindOnce(&RecordAshJobSettings, std::ref(fetched_settings)),
      kInitialJobSettings.Clone());

  // Test that oauth token is in job settings, together with the old settings.
  const base::DictValue kExpectedValue = base::test::ParseJsonDict(R"({
    "key": "value",
    "ipp-client-info": [
      {
        "ipp-client-type": 4,
        "ipp-client-name": "ChromeOS",
        "ipp-client-patches": "patch",
        "ipp-client-string-version": "str_version",
        "ipp-client-version": "version"
      },
      {
        "ipp-client-type": 6,
        "ipp-client-name": "chromebook-42",
        "ipp-client-string-version": "",
      },
    ]
  })");
  EXPECT_EQ(fetched_settings, kExpectedValue);
  EXPECT_EQ(1, local_printer().get_printer_call_count());
  EXPECT_TRUE(ipp_client_info_calculator().get_os_info_called());
  EXPECT_TRUE(ipp_client_info_calculator().get_device_info_called());
}

TEST(LocalPrinterHandlerChromeos, PrinterToValue) {
  base::ScopedMockClockOverride clock_override;
  // Advance to kStatusTimestamp
  clock_override.Advance(kStatusTimestamp - base::Time::Now());
  // Printer status.
  chromeos::CupsPrinterStatus status("printer_id");
  status.AddStatusReason(
      chromeos::CupsPrinterStatus::CupsPrinterStatusReason::Reason::kOutOfInk,
      chromeos::CupsPrinterStatus::CupsPrinterStatusReason::Severity::kWarning);
  // Managed print options.
  chromeos::Printer::ManagedPrintOptions managed_print_options;
  managed_print_options.color.default_value = false;
  managed_print_options.color.allowed_values = {false, true};

  chromeos::Printer printer("device_name");
  printer.set_display_name("printer_name");
  printer.set_description("printer_description");
  printer.set_source(chromeos::Printer::Source::SRC_USER_PREFS);
  printer.set_printer_status(status);
  printer.set_print_job_options(managed_print_options);

  const base::Value kExpectedValue = base::test::ParseJson(R"({
   "cupsEnterprisePrinter": false,
   "deviceName": "device_name",
   "managedPrintOptions": {
      "color": {
        "defaultValue": false,
        "allowedValues": [false, true],
      },
      "dpi": {
         "allowedValues": []
      },
      "duplex": {
         "allowedValues": []
      },
      "mediaSize": {
         "allowedValues": []
      },
      "mediaType": {
         "allowedValues": []
      },
      "printAsImage": {
         "allowedValues": []
      },
      "quality": {
         "allowedValues": []
      },
   },
   "printerDescription": "printer_description",
   "printerName": "printer_name",
   "printerStatus": {
      "printerId": "printer_id",
      "statusReasons": [ {
        "reason": 6,
        "severity": 2
      } ],
      "timestamp": 1e+12
    }
})");
  EXPECT_EQ(kExpectedValue,
            LocalPrinterHandlerChromeos::PrinterToValue(printer));
}

TEST(LocalPrinterHandlerChromeos, PrinterToValue_ConfiguredViaPolicy) {
  chromeos::Printer printer("device_name");
  printer.set_display_name("printer_name");
  printer.set_description("printer_description");
  printer.set_source(chromeos::Printer::Source::SRC_POLICY);

  const base::DictValue printer_dict =
      LocalPrinterHandlerChromeos::PrinterToValue(printer);
  EXPECT_EQ("device_name", *printer_dict.FindString("deviceName"));
  EXPECT_EQ("printer_name", *printer_dict.FindString("printerName"));
  EXPECT_EQ("printer_description",
            *printer_dict.FindString("printerDescription"));
  EXPECT_EQ(true, *printer_dict.FindBool("cupsEnterprisePrinter"));
}

TEST(LocalPrinterHandlerChromeos, ManagedPrintOptionsToValue_MediaSize) {
  chromeos::Printer::ManagedPrintOptions managed_print_options;
  managed_print_options.media_size.default_value =
      chromeos::Printer::Size(5, 10);
  managed_print_options.media_size.allowed_values = {
      chromeos::Printer::Size(5, 10),
      chromeos::Printer::Size(15, 20),
  };

  const base::DictValue managed_print_options_dict =
      LocalPrinterHandlerChromeos::ManagedPrintOptionsToValue(
          managed_print_options);

  EXPECT_EQ(
      *managed_print_options_dict.FindDict(kManagedPrintOptions_MediaSize)
           ->FindDict(kManagedPrintOptions_DefaultValue),
      base::DictValue()
          .Set(kManagedPrintOptions_SizeWidth, 5)
          .Set(kManagedPrintOptions_SizeHeight, 10));
  EXPECT_EQ(
      *managed_print_options_dict.FindDict(kManagedPrintOptions_MediaSize)
           ->FindList(kManagedPrintOptions_AllowedValues),
      base::ListValue()
          .Append(base::DictValue()
                      .Set(kManagedPrintOptions_SizeWidth, 5)
                      .Set(kManagedPrintOptions_SizeHeight, 10))
          .Append(base::DictValue()
                      .Set(kManagedPrintOptions_SizeWidth, 15)
                      .Set(kManagedPrintOptions_SizeHeight, 20)));
}

TEST(LocalPrinterHandlerChromeos, ManagedPrintOptionsToValue_MediaType) {
  chromeos::Printer::ManagedPrintOptions managed_print_options;
  managed_print_options.media_type.default_value = "paper";
  managed_print_options.media_type.allowed_values = {"paper", "metal", "wood"};

  const base::DictValue managed_print_options_dict =
      LocalPrinterHandlerChromeos::ManagedPrintOptionsToValue(
          managed_print_options);

  EXPECT_EQ(
      *managed_print_options_dict.FindDict(kManagedPrintOptions_MediaType)
           ->FindString(kManagedPrintOptions_DefaultValue),
      "paper");
  EXPECT_THAT(
      *managed_print_options_dict.FindDict(kManagedPrintOptions_MediaType)
           ->FindList(kManagedPrintOptions_AllowedValues),
      ElementsAre("paper", "metal", "wood"));
}

TEST(LocalPrinterHandlerChromeos, ManagedPrintOptionsToValue_Duplex) {
  chromeos::Printer::ManagedPrintOptions managed_print_options;
  managed_print_options.duplex.default_value =
      chromeos::Printer::DuplexType::kOneSided;
  managed_print_options.duplex.allowed_values = {
      chromeos::Printer::DuplexType::kOneSided,
      chromeos::Printer::DuplexType::kShortEdge};

  const base::DictValue managed_print_options_dict =
      LocalPrinterHandlerChromeos::ManagedPrintOptionsToValue(
          managed_print_options);

  EXPECT_EQ(*managed_print_options_dict.FindDict(kManagedPrintOptions_Duplex)
                 ->FindInt(kManagedPrintOptions_DefaultValue),
            static_cast<int>(chromeos::Printer::DuplexType::kOneSided));
  EXPECT_THAT(
      *managed_print_options_dict.FindDict(kManagedPrintOptions_Duplex)
           ->FindList(kManagedPrintOptions_AllowedValues),
      ElementsAre(static_cast<int>(chromeos::Printer::DuplexType::kOneSided),
                  static_cast<int>(chromeos::Printer::DuplexType::kShortEdge)));
}

TEST(LocalPrinterHandlerChromeos, ManagedPrintOptionsToValue_Color) {
  chromeos::Printer::ManagedPrintOptions managed_print_options;
  managed_print_options.color.default_value = false;
  managed_print_options.color.allowed_values = {false, true};

  const base::DictValue managed_print_options_dict =
      LocalPrinterHandlerChromeos::ManagedPrintOptionsToValue(
          managed_print_options);

  EXPECT_EQ(*managed_print_options_dict.FindDict(kManagedPrintOptions_Color)
                 ->FindBool(kManagedPrintOptions_DefaultValue),
            false);
  EXPECT_THAT(*managed_print_options_dict.FindDict(kManagedPrintOptions_Color)
                   ->FindList(kManagedPrintOptions_AllowedValues),
              ElementsAre(false, true));
}

TEST(LocalPrinterHandlerChromeos, ManagedPrintOptionsToValue_Dpi) {
  chromeos::Printer::ManagedPrintOptions managed_print_options;
  managed_print_options.dpi.default_value = chromeos::Printer::Dpi(1500, 1000);

  const base::DictValue managed_print_options_dict =
      LocalPrinterHandlerChromeos::ManagedPrintOptionsToValue(
          managed_print_options);

  EXPECT_EQ(*managed_print_options_dict.FindDict(kManagedPrintOptions_Dpi)
                 ->FindDict(kManagedPrintOptions_DefaultValue),
            base::DictValue()
                .Set(kManagedPrintOptions_DpiHorizontal, 1500)
                .Set(kManagedPrintOptions_DpiVertical, 1000));
  EXPECT_EQ(*managed_print_options_dict.FindDict(kManagedPrintOptions_Dpi)
                 ->FindList(kManagedPrintOptions_AllowedValues),
            base::ListValue());
}

TEST(LocalPrinterHandlerChromeos, ManagedPrintOptionsToValue_Quality) {
  chromeos::Printer::ManagedPrintOptions managed_print_options;
  managed_print_options.quality.default_value =
      chromeos::Printer::QualityType::kDraft;
  managed_print_options.quality.allowed_values = {
      chromeos::Printer::QualityType::kDraft,
      chromeos::Printer::QualityType::kHigh};

  const base::DictValue managed_print_options_dict =
      LocalPrinterHandlerChromeos::ManagedPrintOptionsToValue(
          managed_print_options);

  EXPECT_EQ(*managed_print_options_dict.FindDict(kManagedPrintOptions_Quality)
                 ->FindInt(kManagedPrintOptions_DefaultValue),
            static_cast<int>(chromeos::Printer::QualityType::kDraft));
  EXPECT_THAT(
      *managed_print_options_dict.FindDict(kManagedPrintOptions_Quality)
           ->FindList(kManagedPrintOptions_AllowedValues),
      ElementsAre(static_cast<int>(chromeos::Printer::QualityType::kDraft),
                  static_cast<int>(chromeos::Printer::QualityType::kHigh)));
}

TEST(LocalPrinterHandlerChromeos, ManagedPrintOptionsToValue_PrintAsImage) {
  chromeos::Printer::ManagedPrintOptions managed_print_options;
  managed_print_options.print_as_image.default_value = std::nullopt;
  managed_print_options.print_as_image.allowed_values = {false, true};

  const base::DictValue managed_print_options_dict =
      LocalPrinterHandlerChromeos::ManagedPrintOptionsToValue(
          managed_print_options);

  EXPECT_EQ(
      managed_print_options_dict.FindDict(kManagedPrintOptions_PrintAsImage)
          ->FindBool(kManagedPrintOptions_DefaultValue),
      std::nullopt);
  EXPECT_THAT(
      *managed_print_options_dict.FindDict(kManagedPrintOptions_PrintAsImage)
           ->FindList(kManagedPrintOptions_AllowedValues),
      ElementsAre(false, true));
}

TEST(LocalPrinterHandlerChromeos, CapabilityToValue) {
  std::optional<chromeos::Printer> printer(chromeos::Printer("device_name"));
  printer->set_display_name("printer_name");
  printer->set_description("printer_description");

  std::optional<::printing::PrinterSemanticCapsAndDefaults> caps(
      (::printing::PrinterSemanticCapsAndDefaults()));

  const base::Value kExpectedValue = base::test::ParseJson(R"({
   "capabilities": {
     "printer": {
        "color": {
           "option": [ {
              "is_default": true,
              "type": "STANDARD_MONOCHROME",
              "vendor_id": "0"
           } ]
        },
        "copies": {
           "default": 1,
           "max": 1
        },
        "page_orientation": {
           "option": [ {
              "is_default": true,
              "type": "PORTRAIT"
           }, {
              "type": "LANDSCAPE"
           }, {
              "type": "AUTO"
           } ]
        },
        "pin": {
           "supported": false
        },
        "supported_content_type": [ {
           "content_type": "application/pdf"
        } ]
     },
     "version": "1.0"
   },
   "printer": {
      "cupsEnterprisePrinter": false,
      "deviceName": "device_name",
      "printerDescription": "printer_description",
      "printerName": "printer_name",
      "printerOptions": {}
   }
})");
  ASSERT_TRUE(kExpectedValue.is_dict());
  EXPECT_EQ(kExpectedValue.GetDict(),
            LocalPrinterHandlerChromeos::CapabilityToValue(std::move(printer),
                                                           std::move(caps)));
}

TEST(LocalPrinterHandlerChromeos, CapabilityToValue_ConfiguredViaPolicy) {
  std::optional<chromeos::Printer> printer(chromeos::Printer("device_name"));
  printer->set_display_name("printer_name");
  printer->set_description("printer_description");
  printer->set_source(chromeos::Printer::SRC_POLICY);

  std::optional<::printing::PrinterSemanticCapsAndDefaults> caps(
      (::printing::PrinterSemanticCapsAndDefaults()));

  const base::Value kExpectedValue = base::test::ParseJson(R"({
   "capabilities": {
     "printer": {
        "color": {
           "option": [ {
              "is_default": true,
              "type": "STANDARD_MONOCHROME",
              "vendor_id": "0"
           } ]
        },
        "copies": {
           "default": 1,
           "max": 1
        },
        "page_orientation": {
           "option": [ {
              "is_default": true,
              "type": "PORTRAIT"
           }, {
              "type": "LANDSCAPE"
           }, {
              "type": "AUTO"
           } ]
        },
        "pin": {
           "supported": false
        },
        "supported_content_type": [ {
           "content_type": "application/pdf"
        } ]
     },
     "version": "1.0"
   },
   "printer": {
      "cupsEnterprisePrinter": true,
      "deviceName": "device_name",
      "printerDescription": "printer_description",
      "printerName": "printer_name",
      "printerOptions": {}
   }
})");
  ASSERT_TRUE(kExpectedValue.is_dict());
  EXPECT_EQ(kExpectedValue.GetDict(),
            LocalPrinterHandlerChromeos::CapabilityToValue(std::move(printer),
                                                           std::move(caps)));
}

TEST(LocalPrinterHandlerChromeos, CapabilityToValue_EmptyInput) {
  EXPECT_TRUE(
      LocalPrinterHandlerChromeos::CapabilityToValue(std::nullopt, std::nullopt)
          .empty());
}

TEST(LocalPrinterHandlerChromeos, StatusToValue) {
  base::ScopedMockClockOverride clock_override;
  // Advance to kStatusTimestamp
  clock_override.Advance(kStatusTimestamp - base::Time::Now());
  // Printer status.
  chromeos::CupsPrinterStatus status("printer_id");
  status.AddStatusReason(
      chromeos::CupsPrinterStatus::CupsPrinterStatusReason::Reason::kOutOfInk,
      chromeos::CupsPrinterStatus::CupsPrinterStatusReason::Severity::kWarning);
  const base::Value kExpectedValue = base::test::ParseJson(R"({
   "printerId": "printer_id",
   "statusReasons": [ {
      "reason": 6,
      "severity": 2
   } ],
   "timestamp": 1e+12
})");
  EXPECT_EQ(kExpectedValue, LocalPrinterHandlerChromeos::StatusToValue(status));
}

TEST(LocalPrinterHandlerChromeos, RecordDpi) {
  base::HistogramTester histogram_tester;
  printing::PrinterSemanticCapsAndDefaults printer_caps;

  std::optional<chromeos::Printer> printer(chromeos::Printer("device_name"));
  printer->set_display_name("printer_name");
  printer->set_description("printer_description");

  std::optional<::printing::PrinterSemanticCapsAndDefaults> caps(
      (::printing::PrinterSemanticCapsAndDefaults()));
  // Represent DPI values in hex to simplify cross checking the values with the
  // metric output.
  caps->default_dpi = {0x00C8, 0x0190};
  caps->dpis = {
      {0x0064, 0x0064}, {0x00C8, 0x0190}, {0x00C8, 0x01F4}, {0x03E8, 0x03E8}};

  LocalPrinterHandlerChromeos::CapabilityToValue(std::move(printer),
                                                 std::move(caps));

  histogram_tester.ExpectUniqueSample("Printing.CUPS.DPI.Count", /*sample=*/4,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample("Printing.CUPS.DPI.Default",
                                      /*sample=*/0x00C80190,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample("Printing.CUPS.DPI.Min",
                                      /*sample=*/0x00640064,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample("Printing.CUPS.DPI.Max",
                                      /*sample=*/0x03E803E8,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectBucketCount("Printing.CUPS.DPI.AllValues",
                                     /*sample=*/0x00640064,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Printing.CUPS.DPI.AllValues",
                                     /*sample=*/0x00C80190,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Printing.CUPS.DPI.AllValues",
                                     /*sample=*/0x00C801F4,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Printing.CUPS.DPI.AllValues",
                                     /*sample=*/0x03E803E8,
                                     /*expected_count=*/1);
}

}  // namespace printing
