// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_provisioning_ui_handler.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/crosapi/mojom/cert_provisioning.mojom.h"
#include "components/policy/core/browser/cloud/message_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/test_web_ui_listener_observer.h"
#include "crypto/nss_util.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos::cert_provisioning {

namespace {

using ::base::test::RunOnceCallback;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::UnorderedElementsAre;

// Extracted from a X.509 certificate using the command:
// openssl x509 -pubkey -noout -in cert.pem
// and reformatted as a single line.
// This represents a RSA public key.
constexpr char kDerEncodedSpkiBase64[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA1na7r6WiaL5slsyHI7bEpP5ad9ffsz"
    "T0mBi8yc03hJpxaA3/2/"
    "PX7esUdTSGoZr1XVBxjjJc4AypzZKlsPqYKZ+lPHZPpXlp8JVHn8w8+"
    "zmPKl319vVYdJv5AE0HOuJZ6a19fXxItgzoB+"
    "oXgkA0mhyPygJwF3HMJfJHRrkxJ73c23R6kKvKTxqRKswvzTo5O5AzZFLdCe+"
    "GVTJuPo4VToGd+ZhS7QvsY38nAYG57fMnzzs5jjMF042AzzWiMt9gGbeuqCE6LXqFuSJYPo+"
    "TLaN7pwQx68PK5pd/lv58B7jjxCIAai0BX1rV6bl/Am3EukhTSuIcQiTr5c1G4E6bKwIDAQAB";

// Display-formatted version of |kDerEncodedSpkiBase64|. (The number of bits in
// the public exponent used to be calculated by num_bytes*8, now it is the
// actual number of used bits.)
constexpr char kFormattedPublicKey[] = R"(Modulus (2048 bits):
  D6 76 BB AF A5 A2 68 BE 6C 96 CC 87 23 B6 C4 A4
FE 5A 77 D7 DF B3 34 F4 98 18 BC C9 CD 37 84 9A
71 68 0D FF DB F3 D7 ED EB 14 75 34 86 A1 9A F5
5D 50 71 8E 32 5C E0 0C A9 CD 92 A5 B0 FA 98 29
9F A5 3C 76 4F A5 79 69 F0 95 47 9F CC 3C FB 39
8F 2A 5D F5 F6 F5 58 74 9B F9 00 4D 07 3A E2 59
E9 AD 7D 7D 7C 48 B6 0C E8 07 EA 17 82 40 34 9A
1C 8F CA 02 70 17 71 CC 25 F2 47 46 B9 31 27 BD
DC DB 74 7A 90 AB CA 4F 1A 91 2A CC 2F CD 3A 39
3B 90 33 64 52 DD 09 EF 86 55 32 6E 3E 8E 15 4E
81 9D F9 98 52 ED 0B EC 63 7F 27 01 81 B9 ED F3
27 CF 3B 39 8E 33 05 D3 8D 80 CF 35 A2 32 DF 60
19 B7 AE A8 21 3A 2D 7A 85 B9 22 58 3E 8F 93 2D
A3 7B A7 04 31 EB C3 CA E6 97 7F 96 FE 7C 07 B8
E3 C4 22 00 6A 2D 01 5F 5A D5 E9 B9 7F 02 6D C4
BA 48 53 4A E2 1C 42 24 EB E5 CD 46 E0 4E 9B 2B

  Public Exponent (17 bits):
  01 00 01)";

// Test values for creating CertProfile for MockCertProvisioningWorker.
constexpr char kDeviceCertProfileId[] = "device_cert_profile_1";
constexpr char kDeviceCertProfileName[] = "Device Certificate Profile 1";
constexpr char kUserCertProfileId[] = "user_cert_profile_1";
constexpr char kUserCertProfileName[] = "User Certificate Profile 1";

// Recursively visits all strings in |value| and replaces placeholders such as
// "$0" with the corresponding message from |messages|.
void FormatDictRecurse(base::Value* value,
                       const std::vector<std::string>& messages) {
  if (value->is_dict()) {
    for (const auto child : value->GetDict()) {
      FormatDictRecurse(&child.second, messages);
    }
    return;
  }
  if (value->is_list()) {
    for (base::Value& child : value->GetList())
      FormatDictRecurse(&child, messages);
    return;
  }
  if (!value->is_string())
    return;
  for (size_t i = 0; i < messages.size(); ++i) {
    std::string placeholder = std::string("$") + base::NumberToString(i);
    if (value->GetString() != placeholder)
      continue;
    *value = base::Value(messages[i]);
  }
}

// Parses |input| as JSON, replaces string fields that match the placeholder
// format "$0" with the corresponding translated message from |message_ids|.
base::Value FormatJsonDict(std::string_view input,
                           std::vector<std::string> messages) {
  base::Value parsed = base::test::ParseJson(input);
  FormatDictRecurse(&parsed, messages);
  return parsed;
}

// When |all_processes| is a list Value that contains the UI representation of
// certifiate provisioning processes, returns the one that has certProfileId
// |profile_id|.
base::Value GetByProfileId(const base::Value& all_processes,
                           const std::string& profile_id) {
  for (const base::Value& process : all_processes.GetList()) {
    if (profile_id == *process.GetDict().FindString("certProfileId"))
      return process.Clone();
  }
  return base::Value();
}

class FakeMojoCertProvisioning : public crosapi::mojom::CertProvisioning {
 public:
  void AddObserver(mojo::PendingRemote<crosapi::mojom::CertProvisioningObserver>
                       observer) override {
    observer_ = mojo::Remote<crosapi::mojom::CertProvisioningObserver>(
        std::move(observer));
  }

  void GetStatus(GetStatusCallback callback) override {
    std::vector<crosapi::mojom::CertProvisioningProcessStatusPtr> result;
    for (auto& process : status_) {
      result.push_back(process->Clone());
    }
    std::move(callback).Run(std::move(result));
  }

  void UpdateOneProcess(const std::string& cert_profile_id) override {}

  void ResetOneProcess(const std::string& cert_profile_id) override {
    reset_one_process_calls_.push_back(cert_profile_id);
  }

  mojo::Remote<crosapi::mojom::CertProvisioningObserver> observer_;
  std::vector<crosapi::mojom::CertProvisioningProcessStatusPtr> status_;
  std::vector<std::string> reset_one_process_calls_;
};

class CertificateProvisioningUiHandlerTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Required for public key (SubjectPublicKeyInfo) formatting that is being
    // done in the UI handler.
    crypto::EnsureNSSInit();

    testing_profile_ = TestingProfile::Builder().Build();

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(testing_profile_.get()));
    web_ui_.set_web_contents(web_contents_.get());

    auto handler = std::make_unique<CertificateProvisioningUiHandler>(
        &mojo_cert_provisioning_);
    handler_ = handler.get();
    web_ui_.AddMessageHandler(std::move(handler));
  }

  // Use in ASSERT_NO_FATAL_FAILURE.
  void ExtractCertProvisioningProcesses(
      base::Value::List& args,
      base::Value* out_all_processes,
      std::vector<std::string>* out_profile_ids) {
    ASSERT_EQ(1U, args.size());
    ASSERT_TRUE(args[0].is_list());
    *out_all_processes = std::move(args[0]);

    // Extract all profile ids for easier verification.
    if (!out_profile_ids)
      return;
    out_profile_ids->clear();
    for (const base::Value& process : out_all_processes->GetList()) {
      const std::string* profile_id =
          process.GetDict().FindString("certProfileId");
      ASSERT_TRUE(profile_id);
      out_profile_ids->push_back(*profile_id);
    }
  }

  // Use in ASSERT_NO_FATAL_FAILURE.
  void RefreshCertProvisioningProcesses(
      base::Value* out_all_processes,
      std::vector<std::string>* out_profile_ids) {
    content::TestWebUIListenerObserver result_waiter(
        &web_ui_, "certificate-provisioning-processes-changed");

    base::Value::List args;
    web_ui_.HandleReceivedMessage("refreshCertificateProvisioningProcessses",
                                  args);

    result_waiter.Wait();
    ASSERT_NO_FATAL_FAILURE(ExtractCertProvisioningProcesses(
        result_waiter.args(), out_all_processes, out_profile_ids));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // TestingProfileManager testing_profile_manager_;
  std::unique_ptr<TestingProfile> testing_profile_;

  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;

  // Owned by |web_ui_|.
  raw_ptr<CertificateProvisioningUiHandler> handler_ = nullptr;

  FakeMojoCertProvisioning mojo_cert_provisioning_;
};

TEST_F(CertificateProvisioningUiHandlerTest, NoProcesses) {
  base::Value all_processes;
  ASSERT_NO_FATAL_FAILURE(RefreshCertProvisioningProcesses(
      &all_processes, /*out_profile_ids=*/nullptr));
  EXPECT_TRUE(all_processes.GetList().empty());
}

TEST_F(CertificateProvisioningUiHandlerTest, HasOneProcess) {
  auto process_0 = crosapi::mojom::CertProvisioningProcessStatus::New();
  process_0->cert_profile_id = kUserCertProfileId;
  process_0->cert_profile_name = kUserCertProfileName;
  process_0->public_key = base::Base64Decode(kDerEncodedSpkiBase64).value();
  process_0->last_update_time = base::Time();
  process_0->state =
      crosapi::mojom::CertProvisioningProcessState::kKeypairGenerated;
  process_0->did_fail = false;
  process_0->is_device_wide = false;
  process_0->last_backend_server_error = nullptr;
  mojo_cert_provisioning_.status_.push_back(std::move(process_0));

  base::Value all_processes;
  std::vector<std::string> profile_ids;
  ASSERT_NO_FATAL_FAILURE(
      RefreshCertProvisioningProcesses(&all_processes, &profile_ids));
  ASSERT_THAT(profile_ids, ElementsAre(kUserCertProfileId));
  EXPECT_EQ(
      GetByProfileId(all_processes, kUserCertProfileId),
      FormatJsonDict(
          R"({
               "certProfileId": "$0",
               "certProfileName": "$1",
               "isDeviceWide": false,
               "publicKey": "$2",
               "stateId": 1,
               "status": "$3",
               "timeSinceLastUpdate": "",
               "lastUnsuccessfulMessage": ""
             })",
          {kUserCertProfileId, kUserCertProfileName, kFormattedPublicKey,
           l10n_util::GetStringUTF8(
               IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR_WAITING)}));
}

TEST_F(CertificateProvisioningUiHandlerTest, HasTwoProcesses) {
  {
    auto process_0 = crosapi::mojom::CertProvisioningProcessStatus::New();
    process_0->cert_profile_id = kUserCertProfileId;
    process_0->cert_profile_name = kUserCertProfileName;
    process_0->public_key = base::Base64Decode(kDerEncodedSpkiBase64).value();
    process_0->last_update_time = base::Time();
    process_0->state =
        crosapi::mojom::CertProvisioningProcessState::kKeypairGenerated;
    process_0->did_fail = false;
    process_0->is_device_wide = false;
    process_0->last_backend_server_error = nullptr;
    mojo_cert_provisioning_.status_.push_back(std::move(process_0));
  }

  {
    auto process_1 = crosapi::mojom::CertProvisioningProcessStatus::New();
    process_1->cert_profile_id = kDeviceCertProfileId;
    process_1->cert_profile_name = kDeviceCertProfileName;
    process_1->public_key = base::Base64Decode(kDerEncodedSpkiBase64).value();
    process_1->last_update_time = base::Time();
    process_1->state =
        crosapi::mojom::CertProvisioningProcessState::kKeyRegistered;
    process_1->did_fail = true;
    process_1->is_device_wide = true;
    base::Time time1;
    EXPECT_TRUE(base::Time::FromString("15 May 2010 10:00:00 GMT", &time1));
    process_1->last_backend_server_error =
        crosapi::mojom::CertProvisioningBackendServerError::New(
            time1, policy::DM_STATUS_REQUEST_INVALID);
    mojo_cert_provisioning_.status_.push_back(std::move(process_1));
  }

  base::Value all_processes;
  std::vector<std::string> profile_ids;
  ASSERT_NO_FATAL_FAILURE(
      RefreshCertProvisioningProcesses(&all_processes, &profile_ids));
  ASSERT_THAT(profile_ids,
              UnorderedElementsAre(kUserCertProfileId, kDeviceCertProfileId));

  EXPECT_EQ(
      GetByProfileId(all_processes, kUserCertProfileId),
      FormatJsonDict(
          R"({
               "certProfileId": "$0",
               "certProfileName": "$1",
               "isDeviceWide": false,
               "publicKey": "$2",
               "stateId": 1,
               "status": "$3",
               "timeSinceLastUpdate": "",
               "lastUnsuccessfulMessage": ""
             })",
          {kUserCertProfileId, kUserCertProfileName, kFormattedPublicKey,
           l10n_util::GetStringUTF8(
               IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR_WAITING)}));

  std::string last_unsuccessful_message =
      base::UTF16ToUTF8(l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_DMSERVER_ERROR_MESSAGE,
          policy::FormatDeviceManagementStatus(
              policy::DM_STATUS_REQUEST_INVALID),
          u"Sat, 15 May 2010 10:00:00 GMT"));

  // The second process failed, stateId should contain the state before failure,
  // status should contain a failure text.
  EXPECT_EQ(
      GetByProfileId(all_processes, kDeviceCertProfileId),
      FormatJsonDict(
          R"({
               "certProfileId": "$0",
               "certProfileName": "$1",
               "isDeviceWide": true,
               "publicKey": "$2",
               "stateId": 4,
               "status": "$3",
               "timeSinceLastUpdate": "",
               "lastUnsuccessfulMessage": "$4"
             })",
          {kDeviceCertProfileId, kDeviceCertProfileName, kFormattedPublicKey,
           l10n_util::GetStringUTF8(
               IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_FAILURE),
           last_unsuccessful_message}));
}

TEST_F(CertificateProvisioningUiHandlerTest, Updates) {
  base::Value all_processes;
  std::vector<std::string> profile_ids;

  // Perform an initial JS-side initiated refresh so that javascript is
  // considered allowed by the UI handler.
  ASSERT_NO_FATAL_FAILURE(
      RefreshCertProvisioningProcesses(&all_processes, &profile_ids));
  ASSERT_THAT(profile_ids, ElementsAre());
  EXPECT_EQ(1U, handler_->ReadAndResetUiRefreshCountForTesting());

  {
    auto process_0 = crosapi::mojom::CertProvisioningProcessStatus::New();
    process_0->cert_profile_id = kUserCertProfileId;
    process_0->cert_profile_name = kUserCertProfileName;
    process_0->public_key = base::Base64Decode(kDerEncodedSpkiBase64).value();
    process_0->last_update_time = base::Time();
    process_0->state =
        crosapi::mojom::CertProvisioningProcessState::kKeypairGenerated;
    process_0->did_fail = false;
    process_0->is_device_wide = false;
    process_0->last_backend_server_error = nullptr;
    mojo_cert_provisioning_.status_.push_back(std::move(process_0));
  }

  // The mojo service triggers an update.
  content::TestWebUIListenerObserver result_waiter_1(
      &web_ui_, "certificate-provisioning-processes-changed");
  mojo_cert_provisioning_.observer_->OnStateChanged();

  result_waiter_1.Wait();
  EXPECT_EQ(1U, handler_->ReadAndResetUiRefreshCountForTesting());

  ASSERT_NO_FATAL_FAILURE(ExtractCertProvisioningProcesses(
      result_waiter_1.args(), &all_processes, &profile_ids));
  ASSERT_THAT(profile_ids, ElementsAre(kUserCertProfileId));

  EXPECT_EQ(
      GetByProfileId(all_processes, kUserCertProfileId),
      FormatJsonDict(
          R"({
               "certProfileId": "$0",
               "certProfileName": "$1",
               "isDeviceWide": false,
               "publicKey": "$2",
               "stateId": 1,
               "status": "$3",
               "timeSinceLastUpdate": "",
               "lastUnsuccessfulMessage": ""
             })",
          {kUserCertProfileId, kUserCertProfileName, kFormattedPublicKey,
           l10n_util::GetStringUTF8(
               IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR_WAITING)}));

  content::TestWebUIListenerObserver result_waiter_2(
      &web_ui_, "certificate-provisioning-processes-changed");
  mojo_cert_provisioning_.observer_->OnStateChanged();

  result_waiter_2.Wait();
  EXPECT_EQ(1U, handler_->ReadAndResetUiRefreshCountForTesting());

  base::Value all_processes_2;
  ASSERT_NO_FATAL_FAILURE(ExtractCertProvisioningProcesses(
      result_waiter_2.args(), &all_processes_2, /*profile_ids=*/nullptr));
  EXPECT_EQ(all_processes, all_processes_2);
}

TEST_F(CertificateProvisioningUiHandlerTest, ResetsWhenSupported) {
  const std::string kCertProvisioningProcessId = "test";
  base::Value::List args;
  args.Append(kCertProvisioningProcessId);
  web_ui_.HandleReceivedMessage("triggerCertificateProvisioningProcessReset",
                                args);
  EXPECT_THAT(mojo_cert_provisioning_.reset_one_process_calls_,
              ElementsAre(kCertProvisioningProcessId));
}
}  // namespace

}  // namespace chromeos::cert_provisioning
