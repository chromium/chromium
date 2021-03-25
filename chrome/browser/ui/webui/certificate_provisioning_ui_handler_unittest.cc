// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_provisioning_ui_handler.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_scheduler.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_test_helpers.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_worker.h"
#include "chrome/browser/ash/cert_provisioning/mock_cert_provisioning_scheduler.h"
#include "chrome/browser/ash/cert_provisioning/mock_cert_provisioning_worker.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/test_web_ui_listener_observer.h"
#include "crypto/nss_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace cert_provisioning {

namespace {

using ::testing::Return;
using ::testing::ReturnPointee;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::StrictMock;
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

// Display-formatted version of |kDerEncodedSpkiBase64|.
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

  Public Exponent (24 bits):
  01 00 01)";

// Test values for creating CertProfile for MockCertProvisioningWorker.
constexpr char kCertProfileVersion[] = "cert_profile_version_1";
constexpr base::TimeDelta kCertProfileRenewalPeriod =
    base::TimeDelta::FromSeconds(0);
constexpr char kDeviceCertProfileId[] = "device_cert_profile_1";
constexpr char kDeviceCertProfileName[] = "Device Certificate Profile 1";
constexpr char kUserCertProfileId[] = "user_cert_profile_1";
constexpr char kUserCertProfileName[] = "User Certificate Profile 1";

void SetupMockCertProvisioningWorker(
    ash::cert_provisioning::MockCertProvisioningWorker* worker,
    ash::cert_provisioning::CertProvisioningWorkerState state,
    const std::string* public_key,
    ash::cert_provisioning::CertProfile& cert_profile) {
  EXPECT_CALL(*worker, GetState).WillRepeatedly(Return(state));
  EXPECT_CALL(*worker, GetLastUpdateTime).WillRepeatedly(Return(base::Time()));
  EXPECT_CALL(*worker, GetPublicKey).WillRepeatedly(ReturnPointee(public_key));
  ON_CALL(*worker, GetCertProfile).WillByDefault(ReturnRef(cert_profile));
}

// Recursively visits all strings in |value| and replaces placeholders such as
// "$0" with the corresponding message from |messages|.
void FormatDictRecurse(base::Value* value,
                       const std::vector<std::string>& messages) {
  if (value->is_dict()) {
    for (const auto& child : value->DictItems())
      FormatDictRecurse(&child.second, messages);
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
base::Value FormatJsonDict(const base::StringPiece input,
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
    if (profile_id == *process.FindStringKey("certProfileId"))
      return process.Clone();
  }
  return base::Value();
}

class CertificateProvisioningUiHandlerTestBase : public ::testing::Test {
 public:
  explicit CertificateProvisioningUiHandlerTestBase(bool user_is_affiliated)
      : profile_helper_for_testing_(user_is_affiliated) {
    base::Base64Decode(kDerEncodedSpkiBase64, &der_encoded_spki_);

    web_contents_ =
        content::WebContents::Create(content::WebContents::CreateParams(
            profile_helper_for_testing_.GetProfile()));
    web_ui_.set_web_contents(web_contents_.get());

    EXPECT_CALL(scheduler_for_user_, GetWorkers)
        .WillRepeatedly(ReturnRef(user_workers_));
    EXPECT_CALL(scheduler_for_user_, GetFailedCertProfileIds)
        .WillRepeatedly(ReturnRef(user_failed_workers_));
    EXPECT_CALL(scheduler_for_user_, AddObserver(_))
        .WillOnce(SaveArg<0>(&scheduler_observer_for_user_));
    EXPECT_CALL(scheduler_for_user_, RemoveObserver(_)).Times(1);

    if (user_is_affiliated) {
      EXPECT_CALL(scheduler_for_device_, GetWorkers)
          .WillRepeatedly(ReturnRef(device_workers_));
      EXPECT_CALL(scheduler_for_device_, GetFailedCertProfileIds)
          .WillRepeatedly(ReturnRef(device_failed_workers_));
      EXPECT_CALL(scheduler_for_device_, AddObserver(_))
          .WillOnce(SaveArg<0>(&scheduler_observer_for_device_));
      EXPECT_CALL(scheduler_for_device_, RemoveObserver(_)).Times(1);
    }

    auto handler = std::make_unique<CertificateProvisioningUiHandler>(
        GetProfile(), &scheduler_for_user_, &scheduler_for_device_);
    handler_ = handler.get();
    web_ui_.AddMessageHandler(std::move(handler));
  }

  ~CertificateProvisioningUiHandlerTestBase() override {}

  CertificateProvisioningUiHandlerTestBase(
      const CertificateProvisioningUiHandlerTestBase& other) = delete;
  CertificateProvisioningUiHandlerTestBase& operator=(
      const CertificateProvisioningUiHandlerTestBase& other) = delete;

  void SetUp() override {
    // Required for public key (SubjectPublicKeyInfo) formatting that is being
    // done in the UI handler.
    crypto::EnsureNSSInit();
  }

  // Use in ASSERT_NO_FATAL_FAILURE.
  void ExtractCertProvisioningProcesses(
      std::vector<base::Value>& args,
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
      const std::string* profile_id = process.FindStringKey("certProfileId");
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

    base::ListValue args;
    web_ui_.HandleReceivedMessage("refreshCertificateProvisioningProcessses",
                                  &args);

    result_waiter.Wait();
    ASSERT_NO_FATAL_FAILURE(ExtractCertProvisioningProcesses(
        result_waiter.args(), out_all_processes, out_profile_ids));
  }

 protected:
  Profile* GetProfile() { return profile_helper_for_testing_.GetProfile(); }

  std::string der_encoded_spki_;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ash::cert_provisioning::ProfileHelperForTesting profile_helper_for_testing_;

  ash::cert_provisioning::WorkerMap user_workers_;
  base::flat_map<ash::cert_provisioning::CertProfileId,
                 ash::cert_provisioning::FailedWorkerInfo>
      user_failed_workers_;
  StrictMock<ash::cert_provisioning::MockCertProvisioningScheduler>
      scheduler_for_user_;
  ash::cert_provisioning::CertProvisioningSchedulerObserver*
      scheduler_observer_for_user_ = nullptr;

  ash::cert_provisioning::WorkerMap device_workers_;
  base::flat_map<ash::cert_provisioning::CertProfileId,
                 ash::cert_provisioning::FailedWorkerInfo>
      device_failed_workers_;
  StrictMock<ash::cert_provisioning::MockCertProvisioningScheduler>
      scheduler_for_device_;
  ash::cert_provisioning::CertProvisioningSchedulerObserver*
      scheduler_observer_for_device_ = nullptr;

  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;

  // Owned by |web_ui_|.
  CertificateProvisioningUiHandler* handler_;
};

class CertificateProvisioningUiHandlerTest
    : public CertificateProvisioningUiHandlerTestBase {
 public:
  CertificateProvisioningUiHandlerTest()
      : CertificateProvisioningUiHandlerTestBase(/*user_is_affiilated=*/false) {
  }
  ~CertificateProvisioningUiHandlerTest() override = default;
};

class CertificateProvisioningUiHandlerAffiliatedTest
    : public CertificateProvisioningUiHandlerTestBase {
 public:
  CertificateProvisioningUiHandlerAffiliatedTest()
      : CertificateProvisioningUiHandlerTestBase(/*user_is_affiilated=*/true) {}
  ~CertificateProvisioningUiHandlerAffiliatedTest() override = default;
};

TEST_F(CertificateProvisioningUiHandlerTest, NoProcesses) {
  base::Value all_processes;
  ASSERT_NO_FATAL_FAILURE(RefreshCertProvisioningProcesses(
      &all_processes, /*out_profile_ids=*/nullptr));
  EXPECT_TRUE(all_processes.GetList().empty());
}

TEST_F(CertificateProvisioningUiHandlerTest, HasProcesses) {
  ash::cert_provisioning::CertProfile user_cert_profile(
      kUserCertProfileId, kUserCertProfileName, kCertProfileVersion,
      /*is_va_enabled=*/true, kCertProfileRenewalPeriod);
  auto user_cert_worker =
      std::make_unique<ash::cert_provisioning::MockCertProvisioningWorker>();
  SetupMockCertProvisioningWorker(
      user_cert_worker.get(),
      ash::cert_provisioning::CertProvisioningWorkerState::kKeypairGenerated,
      &der_encoded_spki_, user_cert_profile);
  user_workers_[kUserCertProfileId] = std::move(user_cert_worker);

  ash::cert_provisioning::CertProfile device_cert_profile(
      kDeviceCertProfileId, kDeviceCertProfileName, kCertProfileVersion,
      /*is_va_enabled=*/true, kCertProfileRenewalPeriod);
  auto device_cert_worker =
      std::make_unique<ash::cert_provisioning::MockCertProvisioningWorker>();
  SetupMockCertProvisioningWorker(
      device_cert_worker.get(),
      ash::cert_provisioning::CertProvisioningWorkerState::kKeypairGenerated,
      &der_encoded_spki_, device_cert_profile);
  device_workers_[kDeviceCertProfileId] = std::move(device_cert_worker);

  // Only the user worker is expected to be displayed in the UI, because the
  // user is not affiliated.
  base::Value all_processes;
  std::vector<std::string> profile_ids;
  ASSERT_NO_FATAL_FAILURE(
      RefreshCertProvisioningProcesses(&all_processes, &profile_ids));
  ASSERT_THAT(profile_ids, UnorderedElementsAre(kUserCertProfileId));
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
               "timeSinceLastUpdate": ""
             })",
          {kUserCertProfileId, kUserCertProfileName, kFormattedPublicKey,
           l10n_util::GetStringUTF8(
               IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR_WAITING)}));
}

TEST_F(CertificateProvisioningUiHandlerAffiliatedTest, HasProcessesAffiliated) {
  ash::cert_provisioning::CertProfile user_cert_profile(
      kUserCertProfileId, kUserCertProfileName, kCertProfileVersion,
      /*is_va_enabled=*/true, kCertProfileRenewalPeriod);
  auto user_cert_worker =
      std::make_unique<ash::cert_provisioning::MockCertProvisioningWorker>();
  SetupMockCertProvisioningWorker(
      user_cert_worker.get(),
      ash::cert_provisioning::CertProvisioningWorkerState::kKeypairGenerated,
      &der_encoded_spki_, user_cert_profile);
  user_workers_[kUserCertProfileId] = std::move(user_cert_worker);

  ash::cert_provisioning::CertProfile device_cert_profile(
      kDeviceCertProfileId, kDeviceCertProfileName, kCertProfileVersion,
      /*is_va_enabled=*/true, kCertProfileRenewalPeriod);
  auto device_cert_worker =
      std::make_unique<ash::cert_provisioning::MockCertProvisioningWorker>();
  SetupMockCertProvisioningWorker(
      device_cert_worker.get(),
      ash::cert_provisioning::CertProvisioningWorkerState::kFailed,
      &der_encoded_spki_, device_cert_profile);
  device_workers_[kDeviceCertProfileId] = std::move(device_cert_worker);

  // Both user and device-wide workers are expected to be displayed in the UI,
  // because the user is affiliated.
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
               "timeSinceLastUpdate": ""
             })",
          {kUserCertProfileId, kUserCertProfileName, kFormattedPublicKey,
           l10n_util::GetStringUTF8(
               IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR_WAITING)}));
  EXPECT_EQ(
      GetByProfileId(all_processes, kDeviceCertProfileId),
      FormatJsonDict(
          R"({
               "certProfileId": "$0",
               "certProfileName": "$1",
               "isDeviceWide": true,
               "publicKey": "$2",
               "stateId": 10,
               "status": "$3",
               "timeSinceLastUpdate": ""
             })",
          {kDeviceCertProfileId, kDeviceCertProfileName, kFormattedPublicKey,
           l10n_util::GetStringUTF8(
               IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_FAILURE)}));
}

TEST_F(CertificateProvisioningUiHandlerTest, Updates) {
  base::Value all_processes;
  std::vector<std::string> profile_ids;

  // Perform an initial JS-side initiated refresh so that javascript is
  // considered allowed by the UI handler.
  ASSERT_NO_FATAL_FAILURE(
      RefreshCertProvisioningProcesses(&all_processes, &profile_ids));
  ASSERT_THAT(profile_ids, UnorderedElementsAre());
  EXPECT_EQ(1U, handler_->ReadAndResetUiRefreshCountForTesting());

  ash::cert_provisioning::CertProfile user_cert_profile(
      kUserCertProfileId, kUserCertProfileName, kCertProfileVersion,
      /*is_va_enabled=*/true, kCertProfileRenewalPeriod);
  auto user_cert_worker =
      std::make_unique<ash::cert_provisioning::MockCertProvisioningWorker>();
  SetupMockCertProvisioningWorker(
      user_cert_worker.get(),
      ash::cert_provisioning::CertProvisioningWorkerState::kKeypairGenerated,
      &der_encoded_spki_, user_cert_profile);
  user_workers_[kUserCertProfileId] = std::move(user_cert_worker);

  // The user worker triggers an update
  content::TestWebUIListenerObserver result_waiter_1(
      &web_ui_, "certificate-provisioning-processes-changed");

  scheduler_observer_for_user_->OnVisibleStateChanged();

  EXPECT_EQ(1U, handler_->ReadAndResetUiRefreshCountForTesting());
  result_waiter_1.Wait();
  ASSERT_NO_FATAL_FAILURE(ExtractCertProvisioningProcesses(
      result_waiter_1.args(), &all_processes, &profile_ids));

  // Only the user worker is expected to be displayed in the UI, because the
  // user is not affiliated.
  ASSERT_THAT(profile_ids, UnorderedElementsAre(kUserCertProfileId));

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
               "timeSinceLastUpdate": ""
             })",
          {kUserCertProfileId, kUserCertProfileName, kFormattedPublicKey,
           l10n_util::GetStringUTF8(
               IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR_WAITING)}));

  content::TestWebUIListenerObserver result_waiter_2(
      &web_ui_, "certificate-provisioning-processes-changed");
  scheduler_observer_for_user_->OnVisibleStateChanged();
  // Another update does not trigger a UI update for the holdoff time.
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(299));
  EXPECT_EQ(0U, handler_->ReadAndResetUiRefreshCountForTesting());

  // When the holdoff time has elapsed, an UI update is triggered.
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(2));
  EXPECT_EQ(1U, handler_->ReadAndResetUiRefreshCountForTesting());
  result_waiter_2.Wait();

  base::Value all_processes_2;
  ASSERT_NO_FATAL_FAILURE(ExtractCertProvisioningProcesses(
      result_waiter_2.args(), &all_processes_2, /*profile_ids=*/nullptr));
  EXPECT_EQ(all_processes, all_processes_2);
}

}  // namespace

}  // namespace cert_provisioning
}  // namespace chromeos
