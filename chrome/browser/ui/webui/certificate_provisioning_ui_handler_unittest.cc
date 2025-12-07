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
#include "base/notimplemented.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_scheduler.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_worker.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "components/policy/core/browser/cloud/message_util.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
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
constexpr char kDeviceProcessId[] = "00000";
constexpr char kDeviceCertProfileId[] = "device_cert_profile_1";
constexpr char kDeviceCertProfileName[] = "Device Certificate Profile 1";
constexpr char kUserProcessId[] = "11111";
constexpr char kUserCertProfileId[] = "user_cert_profile_1";
constexpr char kUserCertProfileName[] = "User Certificate Profile 1";
constexpr char kFailedDeviceCertProfileId[] = "failed_device_cert_profile_1";
constexpr char kFailedDeviceCertProfileName[] =
    "Failed Device Certificate Profile 1";
constexpr char kFailedUserCertProfileId[] = "failed_user_cert_profile_1";
constexpr char kFailedUserCertProfileName[] =
    "Failed User Certificate Profile 1";
constexpr char kCertProfileVersion[] = "cert_profile_version_1";
constexpr base::TimeDelta kCertProfileRenewalPeriod = base::Days(30);

// Fake failure message used for tests. The exact content of the message can be
// chosen arbitrarily.
const char kFakeFailureMessage[] = "Failure Message";

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
    for (base::Value& child : value->GetList()) {
      FormatDictRecurse(&child, messages);
    }
    return;
  }
  if (!value->is_string()) {
    return;
  }
  for (size_t i = 0; i < messages.size(); ++i) {
    std::string placeholder = std::string("$") + base::NumberToString(i);
    if (value->GetString() != placeholder) {
      continue;
    }
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
    if (profile_id == *process.GetDict().FindString("certProfileId")) {
      return process.Clone();
    }
  }
  return base::Value();
}

// Here after fake implementation of ash::cert_provisioning classes.
// TODO(hidehiko): Consider to generalize these classes and move them to
// c/b/ash/cert_provisioning/ to replace gmocks, which is complexity for
// the long term maintenance.

class FakeCertProvisioningScheduler
    : public ash::cert_provisioning::CertProvisioningScheduler {
 public:
  FakeCertProvisioningScheduler() = default;
  FakeCertProvisioningScheduler(const FakeCertProvisioningScheduler&) = delete;
  FakeCertProvisioningScheduler& operator=(
      const FakeCertProvisioningScheduler&) = delete;
  ~FakeCertProvisioningScheduler() override = default;

  // Allows users of the fake to update the state.
  ash::cert_provisioning::WorkerMap& GetWorkers() { return workers_; }

  base::flat_map<ash::cert_provisioning::CertProfileId,
                 ash::cert_provisioning::FailedWorkerInfo>&
  GetFailedCertProfiles() {
    return failed_cert_profiles_;
  }

  // Notifies observers.
  void NotifyObservers() { callback_list_.Notify(); }

  // ash::cert_provisioning::CertProvisioningScheduler overrides:
  bool UpdateOneWorker(
      const ash::cert_provisioning::CertProfileId& cert_profile_id) override {
    NOTIMPLEMENTED_LOG_ONCE();
    return false;
  }
  void UpdateAllWorkers() override { NOTIMPLEMENTED_LOG_ONCE(); }
  bool ResetOneWorker(
      const ash::cert_provisioning::CertProfileId& cert_profile_id) override {
    auto it = workers_.find(cert_profile_id);
    if (it == workers_.end()) {
      return false;
    }
    if (!it->second->IsWorkerMarkedForReset()) {
      it->second->MarkWorkerForReset();
    }
    return true;
  }
  const ash::cert_provisioning::WorkerMap& GetWorkers() const override {
    return workers_;
  }
  const base::flat_map<ash::cert_provisioning::CertProfileId,
                       ash::cert_provisioning::FailedWorkerInfo>&
  GetFailedCertProfileIds() const override {
    return failed_cert_profiles_;
  }
  base::CallbackListSubscription AddObserver(
      base::RepeatingClosure callback) override {
    return callback_list_.Add(std::move(callback));
  }

 private:
  ash::cert_provisioning::WorkerMap workers_;
  base::flat_map<ash::cert_provisioning::CertProfileId,
                 ash::cert_provisioning::FailedWorkerInfo>
      failed_cert_profiles_;
  base::RepeatingCallbackList<void()> callback_list_;
};

class FakeCertProvisioningWorker
    : public ash::cert_provisioning::CertProvisioningWorker {
 public:
  void set_process_id(std::string process_id) {
    process_id_ = std::move(process_id);
  }
  void set_cert_profile(ash::cert_provisioning::CertProfile cert_profile) {
    cert_profile_ = std::move(cert_profile);
  }
  void set_public_key(std::vector<uint8_t> public_key) {
    public_key_ = std::move(public_key);
  }
  void set_state(ash::cert_provisioning::CertProvisioningWorkerState state) {
    state_ = state;
  }
  void set_last_update_time(base::Time last_update_time) {
    last_update_time_ = last_update_time;
  }
  void set_last_backend_server_error(
      const std::optional<ash::cert_provisioning::BackendServerError>&
          last_backend_server_error) {
    last_backend_server_error_ = last_backend_server_error;
  }

  // ash::cert_provisioning::CertProvisioningWorker:
  void DoStep() override { NOTIMPLEMENTED_LOG_ONCE(); }
  void Stop(
      ash::cert_provisioning::CertProvisioningWorkerState state) override {
    NOTIMPLEMENTED_LOG_ONCE();
  }
  void Pause() override { NOTIMPLEMENTED_LOG_ONCE(); }
  void MarkWorkerForReset() override { reset_ = true; }
  bool IsWaiting() const override {
    NOTIMPLEMENTED_LOG_ONCE();
    return false;
  }
  bool IsWorkerMarkedForReset() const override { return reset_; }

  const std::string& GetProcessId() const override { return process_id_; }
  const ash::cert_provisioning::CertProfile& GetCertProfile() const override {
    return cert_profile_;
  }
  const std::vector<uint8_t>& GetPublicKey() const override {
    return public_key_;
  }
  ash::cert_provisioning::CertProvisioningWorkerState GetState()
      const override {
    return state_;
  }
  ash::cert_provisioning::CertProvisioningWorkerState GetPreviousState()
      const override {
    NOTIMPLEMENTED_LOG_ONCE();
    return ash::cert_provisioning::CertProvisioningWorkerState::kFailed;
  }
  // Returns the time when this worker has been last updated.
  base::Time GetLastUpdateTime() const override { return last_update_time_; }
  const std::optional<ash::cert_provisioning::BackendServerError>&
  GetLastBackendServerError() const override {
    return last_backend_server_error_;
  }

  std::string GetFailureMessageWithPii() const override {
    NOTIMPLEMENTED_LOG_ONCE();
    return std::string();
  }

 private:
  std::string process_id_;
  ash::cert_provisioning::CertProfile cert_profile_;
  std::vector<uint8_t> public_key_;
  ash::cert_provisioning::CertProvisioningWorkerState state_;
  base::Time last_update_time_;
  std::optional<ash::cert_provisioning::BackendServerError>
      last_backend_server_error_;
  bool reset_ = false;
};

class CertificateProvisioningUiHandlerTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());

    // Set up UserManager and an affiliated user with logged in state.
    user_manager_.Reset(std::make_unique<user_manager::UserManagerImpl>(
        std::make_unique<user_manager::FakeUserManagerDelegate>(),
        g_browser_process->local_state(),
        /*cros_settings=*/nullptr));
    const auto account_id = AccountId::FromUserEmailGaiaId(
        TestingProfile::kDefaultProfileUserName, GaiaId("123456"));
    ASSERT_TRUE(user_manager::TestHelper(user_manager_.Get())
                    .AddRegularUser(account_id));
    user_manager_->UserLoggedIn(
        account_id, user_manager::TestHelper::GetFakeUsernameHash(account_id));
    user_manager_->SetUserPolicyStatus(account_id, /*is_managed=*/true,
                                       /*is_affiliated=*/true);

    // Required for public key (SubjectPublicKeyInfo) formatting that is being
    // done in the UI handler.
    crypto::EnsureNSSInit();

    // Create a profile instance corresponding to the logged in user.
    auto* testing_profile = testing_profile_manager_.CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName);
    ash::AnnotatedAccountId::Set(testing_profile, account_id);

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(testing_profile));
    web_ui_.set_web_contents(web_contents_.get());

    auto handler = std::make_unique<CertificateProvisioningUiHandler>(
        &user_scheduler_, &device_scheduler_);
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
    if (!out_profile_ids) {
      return;
    }
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

  user_manager::ScopedUserManager user_manager_;

  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};

  FakeCertProvisioningScheduler user_scheduler_;
  FakeCertProvisioningScheduler device_scheduler_;
  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;

  // Owned by |web_ui_|.
  raw_ptr<CertificateProvisioningUiHandler> handler_ = nullptr;
};

TEST_F(CertificateProvisioningUiHandlerTest, NoProcesses) {
  base::Value all_processes;
  ASSERT_NO_FATAL_FAILURE(RefreshCertProvisioningProcesses(
      &all_processes, /*out_profile_ids=*/nullptr));
  EXPECT_TRUE(all_processes.GetList().empty());
}

TEST_F(CertificateProvisioningUiHandlerTest, OneAliveUserWorker) {
  auto worker = std::make_unique<FakeCertProvisioningWorker>();
  worker->set_process_id(kUserProcessId);
  worker->set_cert_profile(ash::cert_provisioning::CertProfile(
      kUserCertProfileId, kUserCertProfileName, kCertProfileVersion,
      ash::cert_provisioning::KeyType::kRsa,
      /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
      ash::cert_provisioning::ProtocolVersion::kStatic));
  worker->set_public_key(base::Base64Decode(kDerEncodedSpkiBase64).value());
  worker->set_state(
      ash::cert_provisioning::CertProvisioningWorkerState::kKeypairGenerated);
  // Any time should work. Any time in the past is a realistic value.
  base::Time last_update_time;
  ASSERT_TRUE(
      base::Time::FromString("15 May 2010 10:00:00 GMT", &last_update_time));
  worker->set_last_update_time(last_update_time);
  worker->set_last_backend_server_error(
      ash::cert_provisioning::BackendServerError(
          policy::DM_STATUS_REQUEST_INVALID, last_update_time));
  user_scheduler_.GetWorkers().try_emplace(kUserCertProfileId,
                                           std::move(worker));

  base::Value all_processes;
  std::vector<std::string> profile_ids;
  ASSERT_NO_FATAL_FAILURE(
      RefreshCertProvisioningProcesses(&all_processes, &profile_ids));
  ASSERT_THAT(profile_ids, ElementsAre(kUserCertProfileId));
  EXPECT_EQ(
      GetByProfileId(all_processes, kUserCertProfileId),
      FormatJsonDict(
          R"({
               "processId": "$0",
               "certProfileId": "$1",
               "certProfileName": "$2",
               "isDeviceWide": false,
               "publicKey": "$3",
               "stateId": 1,
               "status": "$4",
               "timeSinceLastUpdate": "",
               "lastUnsuccessfulMessage": "$5"
             })",
          {kUserProcessId, kUserCertProfileId, kUserCertProfileName,
           kFormattedPublicKey,
           l10n_util::GetStringUTF8(
               IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR_WAITING),
           base::UTF16ToUTF8(l10n_util::GetStringFUTF16(
               IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_DMSERVER_ERROR_MESSAGE,
               policy::FormatDeviceManagementStatus(
                   policy::DM_STATUS_REQUEST_INVALID),
               u"Sat, 15 May 2010 10:00:00 GMT"))}));
}

TEST_F(CertificateProvisioningUiHandlerTest, OneAliveDeviceWorker) {
  auto worker = std::make_unique<FakeCertProvisioningWorker>();
  worker->set_process_id(kDeviceProcessId);
  worker->set_cert_profile(ash::cert_provisioning::CertProfile(
      kDeviceCertProfileId, kDeviceCertProfileName, kCertProfileVersion,
      ash::cert_provisioning::KeyType::kRsa,
      /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
      ash::cert_provisioning::ProtocolVersion::kStatic));
  worker->set_public_key(base::Base64Decode(kDerEncodedSpkiBase64).value());
  worker->set_state(
      ash::cert_provisioning::CertProvisioningWorkerState::kSignCsrFinished);
  // Any time should work. Any time in the past is a realistic value.
  base::Time last_update_time;
  ASSERT_TRUE(
      base::Time::FromString("15 May 2010 10:00:00 GMT", &last_update_time));
  worker->set_last_update_time(last_update_time);
  worker->set_last_backend_server_error(
      ash::cert_provisioning::BackendServerError(
          policy::DM_STATUS_REQUEST_INVALID, last_update_time));
  device_scheduler_.GetWorkers().try_emplace(kDeviceCertProfileId,
                                             std::move(worker));

  base::Value all_processes;
  std::vector<std::string> profile_ids;
  ASSERT_NO_FATAL_FAILURE(
      RefreshCertProvisioningProcesses(&all_processes, &profile_ids));
  ASSERT_THAT(profile_ids, ElementsAre(kDeviceCertProfileId));
  EXPECT_EQ(
      GetByProfileId(all_processes, kDeviceCertProfileId),
      FormatJsonDict(
          R"({
               "processId": "$0",
               "certProfileId": "$1",
               "certProfileName": "$2",
               "isDeviceWide": true,
               "publicKey": "$3",
               "stateId": 6,
               "status": "$4",
               "timeSinceLastUpdate": "",
               "lastUnsuccessfulMessage": "$5"
             })",
          {kDeviceProcessId, kDeviceCertProfileId, kDeviceCertProfileName,
           kFormattedPublicKey,
           l10n_util::GetStringUTF8(
               IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR_WAITING),
           base::UTF16ToUTF8(l10n_util::GetStringFUTF16(
               IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_DMSERVER_ERROR_MESSAGE,
               policy::FormatDeviceManagementStatus(
                   policy::DM_STATUS_REQUEST_INVALID),
               u"Sat, 15 May 2010 10:00:00 GMT"))}));
}

TEST_F(CertificateProvisioningUiHandlerTest, OneFailedUserWorker) {
  base::Time last_update_time;
  ASSERT_TRUE(
      base::Time::FromString("15 May 2010 10:00:00 GMT", &last_update_time));

  ash::cert_provisioning::FailedWorkerInfo& info =
      user_scheduler_.GetFailedCertProfiles()[kFailedUserCertProfileId];
  info.process_id = kUserProcessId;
  info.state_before_failure =
      ash::cert_provisioning::CertProvisioningWorkerState::kVaChallengeFinished;
  info.public_key = base::Base64Decode(kDerEncodedSpkiBase64).value();
  info.cert_profile_name = kFailedUserCertProfileName;
  info.last_update_time = last_update_time;
  info.failure_message = kFakeFailureMessage;

  base::Value all_processes;
  std::vector<std::string> profile_ids;
  ASSERT_NO_FATAL_FAILURE(
      RefreshCertProvisioningProcesses(&all_processes, &profile_ids));
  ASSERT_THAT(profile_ids, ElementsAre(kFailedUserCertProfileId));
  EXPECT_EQ(GetByProfileId(all_processes, kFailedUserCertProfileId),
            FormatJsonDict(
                R"({
               "processId": "$0",
               "certProfileId": "$1",
               "certProfileName": "$2",
               "isDeviceWide": false,
               "publicKey": "$3",
               "stateId": 3,
               "status": "$4",
               "timeSinceLastUpdate": "",
               "lastUnsuccessfulMessage": ""
             })",
                {kUserProcessId, kFailedUserCertProfileId,
                 kFailedUserCertProfileName, kFormattedPublicKey,
                 base::StrCat({"Failure: ", kFakeFailureMessage})}));
}

TEST_F(CertificateProvisioningUiHandlerTest, OneFailedDeviceWorker) {
  base::Time last_update_time;
  ASSERT_TRUE(
      base::Time::FromString("15 May 2010 10:00:00 GMT", &last_update_time));

  ash::cert_provisioning::FailedWorkerInfo& info =
      device_scheduler_.GetFailedCertProfiles()[kFailedDeviceCertProfileId];
  info.process_id = kDeviceProcessId;
  info.state_before_failure = ash::cert_provisioning::
      CertProvisioningWorkerState::kFinishCsrResponseReceived;
  info.public_key = base::Base64Decode(kDerEncodedSpkiBase64).value();
  info.cert_profile_name = kFailedDeviceCertProfileName;
  info.last_update_time = last_update_time;
  info.failure_message = kFakeFailureMessage;

  base::Value all_processes;
  std::vector<std::string> profile_ids;
  ASSERT_NO_FATAL_FAILURE(
      RefreshCertProvisioningProcesses(&all_processes, &profile_ids));
  ASSERT_THAT(profile_ids, ElementsAre(kFailedDeviceCertProfileId));
  EXPECT_EQ(GetByProfileId(all_processes, kFailedDeviceCertProfileId),
            FormatJsonDict(
                R"({
               "processId": "$0",
               "certProfileId": "$1",
               "certProfileName": "$2",
               "isDeviceWide": true,
               "publicKey": "$3",
               "stateId": 7,
               "status": "$4",
               "timeSinceLastUpdate": "",
               "lastUnsuccessfulMessage": ""
             })",
                {kDeviceProcessId, kFailedDeviceCertProfileId,
                 kFailedDeviceCertProfileName, kFormattedPublicKey,
                 base::StrCat({"Failure: ", kFakeFailureMessage})}));
}

TEST_F(CertificateProvisioningUiHandlerTest, HasTwoProcesses) {
  {
    auto worker = std::make_unique<FakeCertProvisioningWorker>();
    worker->set_process_id(kUserProcessId);
    worker->set_cert_profile(ash::cert_provisioning::CertProfile(
        kUserCertProfileId, kUserCertProfileName, kCertProfileVersion,
        ash::cert_provisioning::KeyType::kRsa,
        /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
        ash::cert_provisioning::ProtocolVersion::kStatic));
    worker->set_public_key(base::Base64Decode(kDerEncodedSpkiBase64).value());
    worker->set_state(
        ash::cert_provisioning::CertProvisioningWorkerState::kKeypairGenerated);
    user_scheduler_.GetWorkers().try_emplace(kUserCertProfileId,
                                             std::move(worker));
  }

  {
    base::Time last_update_time;
    ASSERT_TRUE(
        base::Time::FromString("15 May 2010 10:00:00 GMT", &last_update_time));

    ash::cert_provisioning::FailedWorkerInfo& info =
        device_scheduler_.GetFailedCertProfiles()[kFailedDeviceCertProfileId];
    info.process_id = kDeviceProcessId;
    info.state_before_failure =
        ash::cert_provisioning::CertProvisioningWorkerState::kKeyRegistered;
    info.public_key = base::Base64Decode(kDerEncodedSpkiBase64).value();
    info.cert_profile_name = kFailedDeviceCertProfileName;
    info.last_update_time = last_update_time;
  }

  base::Value all_processes;
  std::vector<std::string> profile_ids;
  ASSERT_NO_FATAL_FAILURE(
      RefreshCertProvisioningProcesses(&all_processes, &profile_ids));
  ASSERT_THAT(profile_ids, UnorderedElementsAre(kUserCertProfileId,
                                                kFailedDeviceCertProfileId));

  EXPECT_EQ(
      GetByProfileId(all_processes, kUserCertProfileId),
      FormatJsonDict(
          R"({
               "processId": "$0",
               "certProfileId": "$1",
               "certProfileName": "$2",
               "isDeviceWide": false,
               "publicKey": "$3",
               "stateId": 1,
               "status": "$4",
               "timeSinceLastUpdate": "",
               "lastUnsuccessfulMessage": ""
             })",
          {kUserProcessId, kUserCertProfileId, kUserCertProfileName,
           kFormattedPublicKey,
           l10n_util::GetStringUTF8(
               IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR_WAITING)}));

  // The second process failed, stateId should contain the state before failure,
  // status should contain a failure text.
  EXPECT_EQ(
      GetByProfileId(all_processes, kFailedDeviceCertProfileId),
      FormatJsonDict(
          R"({
               "processId": "$0",
               "certProfileId": "$1",
               "certProfileName": "$2",
               "isDeviceWide": true,
               "publicKey": "$3",
               "stateId": 4,
               "status": "$4",
               "timeSinceLastUpdate": "",
               "lastUnsuccessfulMessage": ""
             })",
          {kDeviceProcessId, kFailedDeviceCertProfileId,
           kFailedDeviceCertProfileName, kFormattedPublicKey,
           l10n_util::GetStringUTF8(
               IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_FAILURE) +
               ": "}));
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
    auto worker = std::make_unique<FakeCertProvisioningWorker>();
    worker->set_process_id(kUserProcessId);
    worker->set_cert_profile(ash::cert_provisioning::CertProfile(
        kUserCertProfileId, kUserCertProfileName, kCertProfileVersion,
        ash::cert_provisioning::KeyType::kRsa,
        /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
        ash::cert_provisioning::ProtocolVersion::kStatic));
    worker->set_public_key(base::Base64Decode(kDerEncodedSpkiBase64).value());
    worker->set_state(
        ash::cert_provisioning::CertProvisioningWorkerState::kKeypairGenerated);
    user_scheduler_.GetWorkers().try_emplace(kUserCertProfileId,
                                             std::move(worker));
  }

  // The mojo service triggers an update.
  content::TestWebUIListenerObserver result_waiter_1(
      &web_ui_, "certificate-provisioning-processes-changed");
  user_scheduler_.NotifyObservers();

  result_waiter_1.Wait();
  EXPECT_EQ(1U, handler_->ReadAndResetUiRefreshCountForTesting());

  ASSERT_NO_FATAL_FAILURE(ExtractCertProvisioningProcesses(
      result_waiter_1.args(), &all_processes, &profile_ids));
  ASSERT_THAT(profile_ids, ElementsAre(kUserCertProfileId));

  EXPECT_EQ(
      GetByProfileId(all_processes, kUserCertProfileId),
      FormatJsonDict(
          R"({
               "processId": "$0",
               "certProfileId": "$1",
               "certProfileName": "$2",
               "isDeviceWide": false,
               "publicKey": "$3",
               "stateId": 1,
               "status": "$4",
               "timeSinceLastUpdate": "",
               "lastUnsuccessfulMessage": ""
             })",
          {kUserProcessId, kUserCertProfileId, kUserCertProfileName,
           kFormattedPublicKey,
           l10n_util::GetStringUTF8(
               IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR_WAITING)}));

  content::TestWebUIListenerObserver result_waiter_2(
      &web_ui_, "certificate-provisioning-processes-changed");
  user_scheduler_.NotifyObservers();

  result_waiter_2.Wait();
  EXPECT_EQ(1U, handler_->ReadAndResetUiRefreshCountForTesting());

  base::Value all_processes_2;
  ASSERT_NO_FATAL_FAILURE(ExtractCertProvisioningProcesses(
      result_waiter_2.args(), &all_processes_2, /*profile_ids=*/nullptr));
  EXPECT_EQ(all_processes, all_processes_2);
}

TEST_F(CertificateProvisioningUiHandlerTest, ResetsWhenSupported) {
  auto worker = std::make_unique<FakeCertProvisioningWorker>();
  worker->set_process_id(kUserProcessId);
  worker->set_cert_profile(ash::cert_provisioning::CertProfile(
      kUserCertProfileId, kUserCertProfileName, kCertProfileVersion,
      ash::cert_provisioning::KeyType::kRsa,
      /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
      ash::cert_provisioning::ProtocolVersion::kStatic));
  worker->set_public_key(base::Base64Decode(kDerEncodedSpkiBase64).value());
  worker->set_state(
      ash::cert_provisioning::CertProvisioningWorkerState::kKeypairGenerated);
  auto* worker_ptr = worker.get();
  user_scheduler_.GetWorkers().try_emplace(kUserCertProfileId,
                                           std::move(worker));

  base::Value::List args;
  args.Append(kUserCertProfileId);
  web_ui_.HandleReceivedMessage("triggerCertificateProvisioningProcessReset",
                                args);

  EXPECT_TRUE(worker_ptr->IsWorkerMarkedForReset());
}

}  // namespace

}  // namespace chromeos::cert_provisioning
