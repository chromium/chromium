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

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/test/chromeos/printing/fake_local_printer_chromeos.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "printing/backend/print_backend.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings_conversion_chromeos.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

using ::crosapi::mojom::GetOAuthAccessTokenResult;
using ::crosapi::mojom::LocalPrinter;
using ::crosapi::mojom::OAuthNotNeeded;
using ::printing::mojom::IppClientInfo;
using ::printing::mojom::IppClientInfoPtr;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::WithArg;

constexpr auto kStatusTimestamp = base::Time::FromSecondsSinceUnixEpoch(1e9);

// A `LocalPrinter` implementation where all functions run callbacks with
// reasonable default values.
class TestLocalPrinter : public FakeLocalPrinter {
 public:
  void GetUsernamePerPolicy(GetUsernamePerPolicyCallback callback) override {
    std::move(callback).Run(std::nullopt);
  }

  void GetOAuthAccessToken(const std::string& printer_id,
                           GetOAuthAccessTokenCallback callback) override {
    std::move(callback).Run(
        GetOAuthAccessTokenResult::NewNone(OAuthNotNeeded::New()));
  }

  void GetIppClientInfo(const std::string& printer_id,
                        GetIppClientInfoCallback callback) override {
    std::move(callback).Run({});
  }
};

class MockLocalPrinter : public TestLocalPrinter {
 public:
  MOCK_METHOD(void,
              GetUsernamePerPolicy,
              (GetUsernamePerPolicyCallback callback));
  MOCK_METHOD(void,
              GetOAuthAccessToken,
              (const std::string& printer_id,
               GetOAuthAccessTokenCallback callback));
  MOCK_METHOD(void,
              GetIppClientInfo,
              (const std::string& printer_id,
               GetIppClientInfoCallback callback));

  void DelegateToBase() {
    ON_CALL(*this, GetUsernamePerPolicy)
        .WillByDefault([this](GetUsernamePerPolicyCallback cb) {
          return TestLocalPrinter::GetUsernamePerPolicy(std::move(cb));
        });
    ON_CALL(*this, GetOAuthAccessToken)
        .WillByDefault([this](const std::string& printer_id,
                              GetOAuthAccessTokenCallback cb) {
          return TestLocalPrinter::GetOAuthAccessToken(printer_id,
                                                       std::move(cb));
        });
    ON_CALL(*this, GetIppClientInfo)
        .WillByDefault([this](const std::string& printer_id,
                              GetIppClientInfoCallback cb) {
          return TestLocalPrinter::GetIppClientInfo(printer_id, std::move(cb));
        });
  }
};

// Used as a callback to `StartGetPrinters()` in tests.
// Increases `call_count` and records values returned by `StartGetPrinters()`.
void RecordPrinterList(size_t& call_count,
                       base::Value::List& printers_out,
                       base::Value::List printers) {
  ++call_count;
  printers_out = std::move(printers);
}

// Used as a callback to `StartGetPrinters` in tests.
// Records that the test is done.
void RecordPrintersDone(bool& is_done_out) {
  is_done_out = true;
}

void RecordGetCapability(base::Value::Dict& capabilities_out,
                         base::Value::Dict capability) {
  capabilities_out = std::move(capability);
}

void RecordGetEulaUrl(std::string& fetched_eula_url,
                      const std::string& eula_url) {
  fetched_eula_url = eula_url;
}

void RecordAshJobSettings(base::Value::Dict& fetched_settings,
                          base::Value::Dict settings) {
  fetched_settings = std::move(settings);
}

const base::Value::Dict kInitialJobSettings = base::test::ParseJsonDict(R"({
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
        /*local_printer=*/nullptr);
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
    local_printer_handler_ =
        LocalPrinterHandlerChromeos::CreateForTesting(&local_printer_);
  }

  LocalPrinterHandlerChromeos* local_printer_handler() {
    return local_printer_handler_.get();
  }
  MockLocalPrinter& local_printer() { return local_printer_; }

 private:
  NiceMock<MockLocalPrinter> local_printer_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<LocalPrinterHandlerChromeos> local_printer_handler_;
};

TEST_F(LocalPrinterHandlerChromeosNoAshTest,
       PrinterStatusRequest_ProvidesDefaultValue) {
  std::optional<base::Value::Dict> printer_status = base::Value::Dict();
  local_printer_handler()->StartPrinterStatusRequest(
      "printer1",
      base::BindLambdaForTesting([&](std::optional<base::Value::Dict> status) {
        printer_status = std::move(status);
      }));
  EXPECT_EQ(std::nullopt, printer_status);
}

TEST_F(LocalPrinterHandlerChromeosNoAshTest, GetPrinters_ProvidesDefaultValue) {
  size_t call_count = 0;
  base::Value::List printers;
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
  base::Value::Dict fetched_caps;
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
  base::Value::Dict fetched_settings;
  local_printer_handler()->GetAshJobSettingsForTesting(
      "printer1",
      base::BindOnce(&RecordAshJobSettings, std::ref(fetched_settings)),
      kInitialJobSettings.Clone());

  EXPECT_EQ(fetched_settings, kInitialJobSettings);
}

TEST_F(LocalPrinterHandlerChromeosWithAshTest, GetAshJobSettingsEmpty) {
  local_printer().DelegateToBase();
  base::Value::Dict fetched_settings;
  local_printer_handler()->GetAshJobSettingsForTesting(
      "printer1",
      base::BindOnce(&RecordAshJobSettings, std::ref(fetched_settings)),
      kInitialJobSettings.Clone());

  EXPECT_EQ(fetched_settings, kInitialJobSettings);
}

TEST_F(LocalPrinterHandlerChromeosWithAshTest, GetAshJobSettingsUsername) {
  local_printer().DelegateToBase();
  auto return_expected_username =
      [](LocalPrinter::GetUsernamePerPolicyCallback cb) {
        std::move(cb).Run("chronos");
      };
  EXPECT_CALL(local_printer(), GetUsernamePerPolicy)
      .WillOnce(WithArg<0>(Invoke(return_expected_username)));

  base::Value::Dict fetched_settings;
  local_printer_handler()->GetAshJobSettingsForTesting(
      "printer1",
      base::BindOnce(&RecordAshJobSettings, std::ref(fetched_settings)),
      kInitialJobSettings.Clone());

  // Test that `username` and `sendUserInfo` are in job settings, together with
  // the old settings.
  const base::Value::Dict kExpectedValue = base::test::ParseJsonDict(R"({
    "key": "value",
    "username": "chronos",
    "sendUserInfo": true
  })");
  EXPECT_EQ(fetched_settings, kExpectedValue);
}

TEST_F(LocalPrinterHandlerChromeosWithAshTest, GetAshJobSettingsOAuthToken) {
  local_printer().DelegateToBase();
  auto return_expected_oauth_token =
      [](LocalPrinter::GetOAuthAccessTokenCallback cb) {
        std::move(cb).Run(GetOAuthAccessTokenResult::NewToken(
            crosapi::mojom::OAuthAccessToken::New("token")));
      };
  EXPECT_CALL(local_printer(), GetOAuthAccessToken)
      .WillOnce(WithArg<1>(Invoke(return_expected_oauth_token)));

  base::Value::Dict fetched_settings;
  local_printer_handler()->GetAshJobSettingsForTesting(
      "printer1",
      base::BindOnce(&RecordAshJobSettings, std::ref(fetched_settings)),
      kInitialJobSettings.Clone());

  // Test that oauth token is in job settings, together with the old settings.
  const base::Value::Dict kExpectedValue = base::test::ParseJsonDict(R"({
    "key": "value",
    "chromeos-access-oauth-token": "token"
  })");
  EXPECT_EQ(fetched_settings, kExpectedValue);
}

TEST_F(LocalPrinterHandlerChromeosWithAshTest,
       GetAshJobSettingsClientInfoEmptyPrinterId) {
  local_printer().DelegateToBase();
  EXPECT_CALL(local_printer(), GetIppClientInfo).Times(0);

  base::Value::Dict fetched_settings;
  local_printer_handler()->GetAshJobSettingsForTesting(
      "", base::BindOnce(&RecordAshJobSettings, std::ref(fetched_settings)),
      kInitialJobSettings.Clone());

  EXPECT_EQ(fetched_settings, kInitialJobSettings);
}

TEST_F(LocalPrinterHandlerChromeosWithAshTest, GetAshJobSettingsClientInfo) {
  local_printer().DelegateToBase();
  const std::vector<IppClientInfo> expected_client_info{
      {IppClientInfo::ClientType::kOperatingSystem, "ChromeOS", "patch",
       "str_version", "version"},
      {IppClientInfo::ClientType::kOther, "chromebook-42", std::nullopt, "",
       std::nullopt}};
  auto return_expected_client_info =
      [client_info =
           expected_client_info](LocalPrinter::GetIppClientInfoCallback cb) {
        std::vector<IppClientInfoPtr> client_infos_to_send;
        client_infos_to_send.push_back(client_info[0].Clone());
        client_infos_to_send.push_back(client_info[1].Clone());
        std::move(cb).Run(std::move(client_infos_to_send));
      };
  EXPECT_CALL(local_printer(), GetIppClientInfo)
      .WillOnce(WithArg<1>(Invoke(std::move(return_expected_client_info))));

  base::Value::Dict fetched_settings;
  local_printer_handler()->GetAshJobSettingsForTesting(
      "printer1",
      base::BindOnce(&RecordAshJobSettings, std::ref(fetched_settings)),
      kInitialJobSettings.Clone());

  // Test that oauth token is in job settings, together with the old settings.
  const base::Value::Dict kExpectedValue = base::test::ParseJsonDict(R"({
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
}

TEST(LocalPrinterHandlerChromeos, PrinterToValue) {
  crosapi::mojom::PrinterStatusPtr status =
      crosapi::mojom::PrinterStatus::New();
  status->printer_id = "printer_id";
  status->timestamp = kStatusTimestamp;
  status->status_reasons.push_back(crosapi::mojom::StatusReason::New(
      crosapi::mojom::StatusReason::Reason::kOutOfInk,
      crosapi::mojom::StatusReason::Severity::kWarning));
  crosapi::mojom::LocalDestinationInfo input("device_name", "printer_name",
                                             "printer_description", false, "",
                                             std::move(status));
  const base::Value kExpectedValue = base::test::ParseJson(R"({
   "cupsEnterprisePrinter": false,
   "deviceName": "device_name",
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
  EXPECT_EQ(kExpectedValue, LocalPrinterHandlerChromeos::PrinterToValue(input));
}

TEST(LocalPrinterHandlerChromeos, PrinterToValue_ConfiguredViaPolicy) {
  crosapi::mojom::LocalDestinationInfo printer("device_name", "printer_name",
                                               "printer_description", true);
  const base::Value kExpectedValue = base::test::ParseJson(R"({
   "cupsEnterprisePrinter": true,
   "deviceName": "device_name",
   "printerDescription": "printer_description",
   "printerName": "printer_name",
   "printerStatus": {}
})");
  EXPECT_EQ(kExpectedValue,
            LocalPrinterHandlerChromeos::PrinterToValue(printer));
}

TEST(LocalPrinterHandlerChromeos, CapabilityToValue) {
  auto caps = crosapi::mojom::CapabilitiesResponse::New();
  caps->basic_info = crosapi::mojom::LocalDestinationInfo::New(
      "device_name", "printer_name", "printer_description", false);

  const base::Value kExpectedValue = base::test::ParseJson(R"({
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
            LocalPrinterHandlerChromeos::CapabilityToValue(std::move(caps)));
}

TEST(LocalPrinterHandlerChromeos, CapabilityToValue_ConfiguredViaPolicy) {
  auto caps = crosapi::mojom::CapabilitiesResponse::New();
  caps->basic_info = crosapi::mojom::LocalDestinationInfo::New(
      "device_name", "printer_name", "printer_description", true);

  const base::Value kExpectedValue = base::test::ParseJson(R"({
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
            LocalPrinterHandlerChromeos::CapabilityToValue(std::move(caps)));
}

TEST(LocalPrinterHandlerChromeos, CapabilityToValue_EmptyInput) {
  EXPECT_TRUE(LocalPrinterHandlerChromeos::CapabilityToValue(nullptr).empty());
}

TEST(LocalPrinterHandlerChromeos, StatusToValue) {
  crosapi::mojom::PrinterStatus status;
  status.printer_id = "printer_id";
  status.timestamp = kStatusTimestamp;
  status.status_reasons.push_back(crosapi::mojom::StatusReason::New(
      crosapi::mojom::StatusReason::Reason::kOutOfInk,
      crosapi::mojom::StatusReason::Severity::kWarning));
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

  // Represent DPI values in hex to simplify cross checking the values with the
  // metric output.
  printer_caps.default_dpi = {0x00C8, 0x0190};
  printer_caps.dpis = {
      {0x0064, 0x0064}, {0x00C8, 0x0190}, {0x00C8, 0x01F4}, {0x03E8, 0x03E8}};

  auto caps = crosapi::mojom::CapabilitiesResponse::New();
  caps->basic_info = crosapi::mojom::LocalDestinationInfo::New(
      "device_name", "printer_name", "printer_description", false);
  caps->capabilities = printer_caps;
  LocalPrinterHandlerChromeos::CapabilityToValue(std::move(caps));

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
