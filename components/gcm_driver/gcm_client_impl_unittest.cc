// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_client_impl.h"

#include <stdint.h>

#include <initializer_list>
#include <memory>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "components/gcm_driver/features.h"
#include "google_apis/gcm/base/fake_encryptor.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/engine/fake_connection_factory.h"
#include "google_apis/gcm/engine/fake_connection_handler.h"
#include "google_apis/gcm/engine/gservices_settings.h"
#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"
#include "google_apis/gcm/protocol/android_checkin.pb.h"
#include "google_apis/gcm/protocol/checkin.pb.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/test/gtest_util.h"
#include "net/test/scoped_disable_exit_on_dfatal.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

namespace gcm {
namespace {

enum LastEvent {
  NONE,
  LOADING_COMPLETED,
  REGISTRATION_COMPLETED,
  UNREGISTRATION_COMPLETED,
  MESSAGE_SEND_ERROR,
  MESSAGE_SEND_ACK,
  MESSAGE_RECEIVED,
  MESSAGES_DELETED,
};

const char kChromeVersion[] = "45.0.0.1";
const uint64_t kDeviceAndroidId = 54321;
const uint64_t kDeviceSecurityToken = 12345;
const uint64_t kDeviceAndroidId2 = 11111;
const uint64_t kDeviceSecurityToken2 = 2222;
const int64_t kSettingsCheckinInterval = 16 * 60 * 60;
const char kProductCategoryForSubtypes[] = "com.chrome.macosx";
const char kExtensionAppId[] = "abcdefghijklmnopabcdefghijklmnop";
const char kRegistrationId[] = "reg_id";
const char kSubtypeAppId[] = "app_id";
const char kSender[] = "project_id";
const char kSender2[] = "project_id2";
const char kRegistrationResponsePrefix[] = "token=";
const char kUnregistrationResponsePrefix[] = "deleted=";
const char kRawData[] = "example raw data";

const char kInstanceID[] = "iid_1";
const char kScope[] = "GCM";
const char kDeleteTokenResponse[] = "token=foo";
const int kTestTokenInvalidationPeriod = 5;
const char kMessageId[] = "0:12345%5678";

const char kRegisterUrl[] = "https://android.clients.google.com/c2dm/register3";

// Helper for building arbitrary data messages.
MCSMessage BuildDownstreamMessage(
    const std::string& project_id,
    const std::string& category,
    const std::string& subtype,
    const std::map<std::string, std::string>& data,
    const std::string& raw_data) {
  mcs_proto::DataMessageStanza data_message;
  data_message.set_from(project_id);
  data_message.set_category(category);
  for (auto iter = data.begin(); iter != data.end(); ++iter) {
    mcs_proto::AppData* app_data = data_message.add_app_data();
    app_data->set_key(iter->first);
    app_data->set_value(iter->second);
  }
  if (!subtype.empty()) {
    mcs_proto::AppData* app_data = data_message.add_app_data();
    app_data->set_key("subtype");
    app_data->set_value(subtype);
  }
  data_message.set_raw_data(raw_data);
  data_message.set_persistent_id(kMessageId);
  return MCSMessage(kDataMessageStanzaTag, data_message);
}

GCMClient::AccountTokenInfo MakeAccountToken(const std::string& email,
                                             const std::string& token) {
  GCMClient::AccountTokenInfo account_token;
  account_token.email = email;
  account_token.access_token = token;
  return account_token;
}

std::map<std::string, std::string> MakeEmailToTokenMap(
    const std::vector<GCMClient::AccountTokenInfo>& account_tokens) {
  std::map<std::string, std::string> email_token_map;
  for (auto iter = account_tokens.begin(); iter != account_tokens.end();
       ++iter) {
    email_token_map[iter->email] = iter->access_token;
  }
  return email_token_map;
}

class FakeMCSClient : public MCSClient {
 public:
  FakeMCSClient(base::Clock* clock,
                ConnectionFactory* connection_factory,
                GCMStore* gcm_store,
                scoped_refptr<base::SequencedTaskRunner> io_task_runner,
                GCMStatsRecorder* recorder);
  ~FakeMCSClient() override;
  void Login(uint64_t android_id, uint64_t security_token) override;
  void SendMessage(const MCSMessage& message) override;

  uint64_t last_android_id() const { return last_android_id_; }
  uint64_t last_security_token() const { return last_security_token_; }
  uint8_t last_message_tag() const { return last_message_tag_; }
  const mcs_proto::DataMessageStanza& last_data_message_stanza() const {
    return last_data_message_stanza_;
  }

 private:
  uint64_t last_android_id_;
  uint64_t last_security_token_;
  uint8_t last_message_tag_;
  mcs_proto::DataMessageStanza last_data_message_stanza_;
};

FakeMCSClient::FakeMCSClient(
    base::Clock* clock,
    ConnectionFactory* connection_factory,
    GCMStore* gcm_store,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    GCMStatsRecorder* recorder)
    : MCSClient("",
                clock,
                connection_factory,
                gcm_store,
                io_task_runner,
                recorder),
      last_android_id_(0u),
      last_security_token_(0u),
      last_message_tag_(kNumProtoTypes) {}

FakeMCSClient::~FakeMCSClient() {
}

void FakeMCSClient::Login(uint64_t android_id, uint64_t security_token) {
  last_android_id_ = android_id;
  last_security_token_ = security_token;
}

void FakeMCSClient::SendMessage(const MCSMessage& message) {
  last_message_tag_ = message.tag();
  if (last_message_tag_ == kDataMessageStanzaTag) {
    last_data_message_stanza_.CopyFrom(
        reinterpret_cast<const mcs_proto::DataMessageStanza&>(
            message.GetProtobuf()));
  }
}

class AutoAdvancingTestClock : public base::Clock {
 public:
  explicit AutoAdvancingTestClock(base::TimeDelta auto_increment_time_delta);
  ~AutoAdvancingTestClock() override;

  base::Time Now() const override;
  void Advance(base::TimeDelta delta);
  int call_count() const { return call_count_; }

 private:
  mutable int call_count_;
  base::TimeDelta auto_increment_time_delta_;
  mutable base::Time now_;

  DISALLOW_COPY_AND_ASSIGN(AutoAdvancingTestClock);
};

AutoAdvancingTestClock::AutoAdvancingTestClock(
    base::TimeDelta auto_increment_time_delta)
    : call_count_(0), auto_increment_time_delta_(auto_increment_time_delta) {
}

AutoAdvancingTestClock::~AutoAdvancingTestClock() {
}

base::Time AutoAdvancingTestClock::Now() const {
  call_count_++;
  now_ += auto_increment_time_delta_;
  return now_;
}

void AutoAdvancingTestClock::Advance(base::TimeDelta delta) {
  now_ += delta;
}

class FakeGCMInternalsBuilder : public GCMInternalsBuilder {
 public:
  explicit FakeGCMInternalsBuilder(base::TimeDelta clock_step);
  ~FakeGCMInternalsBuilder() override;

  base::Clock* GetClock() override;
  std::unique_ptr<MCSClient> BuildMCSClient(
      const std::string& version,
      base::Clock* clock,
      ConnectionFactory* connection_factory,
      GCMStore* gcm_store,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      GCMStatsRecorder* recorder) override;
  std::unique_ptr<ConnectionFactory> BuildConnectionFactory(
      const std::vector<GURL>& endpoints,
      const net::BackoffEntry::Policy& backoff_policy,
      base::RepeatingCallback<void(
          mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
          get_socket_factory_callback,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      GCMStatsRecorder* recorder,
      network::NetworkConnectionTracker* network_connection_tracker) override;

 private:
  AutoAdvancingTestClock clock_;
};

FakeGCMInternalsBuilder::FakeGCMInternalsBuilder(base::TimeDelta clock_step)
    : clock_(clock_step) {}

FakeGCMInternalsBuilder::~FakeGCMInternalsBuilder() {}

base::Clock* FakeGCMInternalsBuilder::GetClock() {
  return &clock_;
}

std::unique_ptr<MCSClient> FakeGCMInternalsBuilder::BuildMCSClient(
    const std::string& version,
    base::Clock* clock,
    ConnectionFactory* connection_factory,
    GCMStore* gcm_store,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    GCMStatsRecorder* recorder) {
  return base::WrapUnique<MCSClient>(new FakeMCSClient(
      clock, connection_factory, gcm_store, io_task_runner, recorder));
}

std::unique_ptr<ConnectionFactory>
FakeGCMInternalsBuilder::BuildConnectionFactory(
    const std::vector<GURL>& endpoints,
    const net::BackoffEntry::Policy& backoff_policy,
    base::RepeatingCallback<void(
        mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
        get_socket_factory_callback,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    GCMStatsRecorder* recorder,
    network::NetworkConnectionTracker* network_connection_tracker) {
  return base::WrapUnique<ConnectionFactory>(new FakeConnectionFactory());
}

}  // namespace

class GCMClientImplTest : public testing::Test,
                          public GCMClient::Delegate {
 public:
  GCMClientImplTest();
  ~GCMClientImplTest() override;

  void SetUp() override;
  void TearDown() override;

  void SetFeatureParams(const base::Feature& feature,
                        const base::FieldTrialParams& params);

  void InitializeInvalidationFieldTrial();

  void BuildGCMClient(base::TimeDelta clock_step);
  void InitializeGCMClient();
  void StartGCMClient();
  void Register(const std::string& app_id,
                const std::vector<std::string>& senders);
  void Unregister(const std::string& app_id);
  void ReceiveMessageFromMCS(const MCSMessage& message);
  void ReceiveOnMessageSentToMCS(
      const std::string& app_id,
      const std::string& message_id,
      const MCSClient::MessageSendStatus status);
  void FailCheckin(net::HttpStatusCode response_code);
  void CompleteCheckin(uint64_t android_id,
                       uint64_t security_token,
                       const std::string& digest,
                       const std::map<std::string, std::string>& settings);
  void CompleteCheckinImpl(uint64_t android_id,
                           uint64_t security_token,
                           const std::string& digest,
                           const std::map<std::string, std::string>& settings,
                           net::HttpStatusCode response_code);
  void CompleteRegistration(const std::string& registration_id);
  void CompleteUnregistration(const std::string& app_id);

  bool ExistsRegistration(const std::string& app_id) const;
  void AddRegistration(const std::string& app_id,
                       const std::vector<std::string>& sender_ids,
                       const std::string& registration_id);

  // GCMClient::Delegate overrides (for verification).
  void OnRegisterFinished(scoped_refptr<RegistrationInfo> registration_info,
                          const std::string& registration_id,
                          GCMClient::Result result) override;
  void OnUnregisterFinished(scoped_refptr<RegistrationInfo> registration_info,
                            GCMClient::Result result) override;
  void OnSendFinished(const std::string& app_id,
                      const std::string& message_id,
                      GCMClient::Result result) override {}
  void OnMessageReceived(const std::string& registration_id,
                         const IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnMessageSendError(
      const std::string& app_id,
      const gcm::GCMClient::SendErrorDetails& send_error_details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;
  void OnGCMReady(const std::vector<AccountMapping>& account_mappings,
                  const base::Time& last_token_fetch_time) override;
  void OnActivityRecorded() override {}
  void OnConnected(const net::IPEndPoint& ip_endpoint) override {}
  void OnDisconnected() override {}
  void OnStoreReset() override {}

  GCMClientImpl* gcm_client() const { return gcm_client_.get(); }
  GCMClientImpl::State gcm_client_state() const {
    return gcm_client_->state_;
  }
  FakeMCSClient* mcs_client() const {
    return reinterpret_cast<FakeMCSClient*>(gcm_client_->mcs_client_.get());
  }
  ConnectionFactory* connection_factory() const {
    return gcm_client_->connection_factory_.get();
  }

  const GCMClientImpl::CheckinInfo& device_checkin_info() const {
    return gcm_client_->device_checkin_info_;
  }

  void reset_last_event() {
    last_event_ = NONE;
    last_app_id_.clear();
    last_registration_id_.clear();
    last_message_id_.clear();
    last_result_ = GCMClient::UNKNOWN_ERROR;
    last_account_mappings_.clear();
    last_token_fetch_time_ = base::Time();
  }

  LastEvent last_event() const { return last_event_; }
  const std::string& last_app_id() const { return last_app_id_; }
  const std::string& last_registration_id() const {
    return last_registration_id_;
  }
  const std::string& last_message_id() const { return last_message_id_; }
  GCMClient::Result last_result() const { return last_result_; }
  const IncomingMessage& last_message() const { return last_message_; }
  const GCMClient::SendErrorDetails& last_error_details() const {
    return last_error_details_;
  }
  const base::Time& last_token_fetch_time() const {
    return last_token_fetch_time_;
  }
  const std::vector<AccountMapping>& last_account_mappings() {
    return last_account_mappings_;
  }

  const GServicesSettings& gservices_settings() const {
    return gcm_client_->gservices_settings_;
  }

  const base::FilePath& temp_directory_path() const {
    return temp_directory_.GetPath();
  }

  base::FilePath gcm_store_path() const {
    // Pass an non-existent directory as store path to match the exact
    // behavior in the production code. Currently GCMStoreImpl checks if
    // the directory exist or not to determine the store existence.
    return temp_directory_.GetPath().Append(FILE_PATH_LITERAL("GCM Store"));
  }

  int64_t CurrentTime();

  // Tooling.
  void PumpLoopUntilIdle();
  bool CreateUniqueTempDir();
  AutoAdvancingTestClock* clock() const {
    return static_cast<AutoAdvancingTestClock*>(gcm_client_->clock_);
  }
  network::TestURLLoaderFactory* url_loader_factory() {
    return &test_url_loader_factory_;
  }

  void FastForwardBy(const base::TimeDelta& duration) {
    task_environment_.FastForwardBy(duration);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Must be declared first so that it is destroyed last. Injected to
  // GCM client.
  base::ScopedTempDir temp_directory_;

  // Variables used for verification.
  LastEvent last_event_;
  std::string last_app_id_;
  std::string last_registration_id_;
  std::string last_message_id_;
  GCMClient::Result last_result_;
  IncomingMessage last_message_;
  GCMClient::SendErrorDetails last_error_details_;
  base::Time last_token_fetch_time_;
  std::vector<AccountMapping> last_account_mappings_;

  std::unique_ptr<GCMClientImpl> gcm_client_;

  // Injected to GCM client.
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

GCMClientImplTest::GCMClientImplTest()
    : last_event_(NONE), last_result_(GCMClient::UNKNOWN_ERROR) {}

GCMClientImplTest::~GCMClientImplTest() {}

void GCMClientImplTest::SetUp() {
  testing::Test::SetUp();
  ASSERT_TRUE(CreateUniqueTempDir());
  BuildGCMClient(base::TimeDelta());
  InitializeGCMClient();
  StartGCMClient();
  InitializeInvalidationFieldTrial();
  ASSERT_NO_FATAL_FAILURE(
      CompleteCheckin(kDeviceAndroidId, kDeviceSecurityToken, std::string(),
                      std::map<std::string, std::string>()));
}

void GCMClientImplTest::TearDown() {
}

void GCMClientImplTest::SetFeatureParams(const base::Feature& feature,
                                         const base::FieldTrialParams& params) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(feature, params);

  base::FieldTrialParams actual_params;
  EXPECT_TRUE(base::GetFieldTrialParamsByFeature(
      features::kInvalidateTokenFeature, &actual_params));
  EXPECT_EQ(params, actual_params);
}

void GCMClientImplTest::InitializeInvalidationFieldTrial() {
  std::map<std::string, std::string> params;
  params[features::kParamNameTokenInvalidationPeriodDays] =
      std::to_string(kTestTokenInvalidationPeriod);
  ASSERT_NO_FATAL_FAILURE(
      SetFeatureParams(features::kInvalidateTokenFeature, std::move(params)));
}

void GCMClientImplTest::PumpLoopUntilIdle() {
  task_environment_.RunUntilIdle();
}

bool GCMClientImplTest::CreateUniqueTempDir() {
  return temp_directory_.CreateUniqueTempDir();
}

void GCMClientImplTest::BuildGCMClient(base::TimeDelta clock_step) {
  gcm_client_.reset(new GCMClientImpl(base::WrapUnique<GCMInternalsBuilder>(
      new FakeGCMInternalsBuilder(clock_step))));
}

void GCMClientImplTest::FailCheckin(net::HttpStatusCode response_code) {
  std::map<std::string, std::string> settings;
  CompleteCheckinImpl(0, 0, GServicesSettings::CalculateDigest(settings),
                      settings, response_code);
}

void GCMClientImplTest::CompleteCheckin(
    uint64_t android_id,
    uint64_t security_token,
    const std::string& digest,
    const std::map<std::string, std::string>& settings) {
  CompleteCheckinImpl(android_id, security_token, digest, settings,
                      net::HTTP_OK);
}

void GCMClientImplTest::CompleteCheckinImpl(
    uint64_t android_id,
    uint64_t security_token,
    const std::string& digest,
    const std::map<std::string, std::string>& settings,
    net::HttpStatusCode response_code) {
  checkin_proto::AndroidCheckinResponse response;
  response.set_stats_ok(true);
  response.set_android_id(android_id);
  response.set_security_token(security_token);

  // For testing G-services settings.
  if (!digest.empty()) {
    response.set_digest(digest);
    for (auto it = settings.begin(); it != settings.end(); ++it) {
      checkin_proto::GservicesSetting* setting = response.add_setting();
      setting->set_name(it->first);
      setting->set_value(it->second);
    }
    response.set_settings_diff(false);
  }

  std::string response_string;
  response.SerializeToString(&response_string);

  EXPECT_TRUE(url_loader_factory()->SimulateResponseForPendingRequest(
      gservices_settings().GetCheckinURL(),
      network::URLLoaderCompletionStatus(net::OK),
      network::CreateURLResponseHead(response_code), response_string));
  // Give a chance for GCMStoreImpl::Backend to finish persisting data.
  PumpLoopUntilIdle();
}

void GCMClientImplTest::CompleteRegistration(
    const std::string& registration_id) {
  std::string response(kRegistrationResponsePrefix);
  response.append(registration_id);

  EXPECT_TRUE(url_loader_factory()->SimulateResponseForPendingRequest(
      GURL(kRegisterUrl), network::URLLoaderCompletionStatus(net::OK),
      network::CreateURLResponseHead(net::HTTP_OK), response));

  // Give a chance for GCMStoreImpl::Backend to finish persisting data.
  PumpLoopUntilIdle();
}

void GCMClientImplTest::CompleteUnregistration(
    const std::string& app_id) {
  std::string response(kUnregistrationResponsePrefix);
  response.append(app_id);

  EXPECT_TRUE(url_loader_factory()->SimulateResponseForPendingRequest(
      GURL(kRegisterUrl), network::URLLoaderCompletionStatus(net::OK),
      network::CreateURLResponseHead(net::HTTP_OK), response));

  // Give a chance for GCMStoreImpl::Backend to finish persisting data.
  PumpLoopUntilIdle();
}

bool GCMClientImplTest::ExistsRegistration(const std::string& app_id) const {
  return ExistsGCMRegistrationInMap(gcm_client_->registrations_, app_id);
}

void GCMClientImplTest::AddRegistration(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids,
    const std::string& registration_id) {
  auto registration = base::MakeRefCounted<GCMRegistrationInfo>();
  registration->app_id = app_id;
  registration->sender_ids = sender_ids;
  gcm_client_->registrations_.emplace(std::move(registration), registration_id);
}

void GCMClientImplTest::InitializeGCMClient() {
  clock()->Advance(base::TimeDelta::FromMilliseconds(1));

  // Actual initialization.
  GCMClient::ChromeBuildInfo chrome_build_info;
  chrome_build_info.version = kChromeVersion;
  chrome_build_info.product_category_for_subtypes = kProductCategoryForSubtypes;
  gcm_client_->Initialize(
      chrome_build_info, gcm_store_path(),
      task_environment_.GetMainThreadTaskRunner(),
      base::ThreadTaskRunnerHandle::Get(), base::DoNothing(),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_),
      network::TestNetworkConnectionTracker::GetInstance(),
      base::WrapUnique<Encryptor>(new FakeEncryptor), this);
}

void GCMClientImplTest::StartGCMClient() {
  // Start loading and check-in.
  gcm_client_->Start(GCMClient::IMMEDIATE_START);

  PumpLoopUntilIdle();
}

void GCMClientImplTest::Register(const std::string& app_id,
                                 const std::vector<std::string>& senders) {
  auto gcm_info = base::MakeRefCounted<GCMRegistrationInfo>();
  gcm_info->app_id = app_id;
  gcm_info->sender_ids = senders;
  gcm_client()->Register(std::move(gcm_info));
}

void GCMClientImplTest::Unregister(const std::string& app_id) {
  auto gcm_info = base::MakeRefCounted<GCMRegistrationInfo>();
  gcm_info->app_id = app_id;
  gcm_client()->Unregister(std::move(gcm_info));
}

void GCMClientImplTest::ReceiveMessageFromMCS(const MCSMessage& message) {
  gcm_client_->recorder_.RecordConnectionInitiated(std::string());
  gcm_client_->recorder_.RecordConnectionSuccess();
  gcm_client_->OnMessageReceivedFromMCS(message);
}

void GCMClientImplTest::ReceiveOnMessageSentToMCS(
      const std::string& app_id,
      const std::string& message_id,
      const MCSClient::MessageSendStatus status) {
  gcm_client_->OnMessageSentToMCS(0LL, app_id, message_id, status);
}

void GCMClientImplTest::OnGCMReady(
    const std::vector<AccountMapping>& account_mappings,
    const base::Time& last_token_fetch_time) {
  last_event_ = LOADING_COMPLETED;
  last_account_mappings_ = account_mappings;
  last_token_fetch_time_ = last_token_fetch_time;
}

void GCMClientImplTest::OnMessageReceived(const std::string& registration_id,
                                          const IncomingMessage& message) {
  last_event_ = MESSAGE_RECEIVED;
  last_app_id_ = registration_id;
  last_message_ = message;
}

void GCMClientImplTest::OnRegisterFinished(
    scoped_refptr<RegistrationInfo> registration_info,
    const std::string& registration_id,
    GCMClient::Result result) {
  last_event_ = REGISTRATION_COMPLETED;
  last_app_id_ = registration_info->app_id;
  last_registration_id_ = registration_id;
  last_result_ = result;
}

void GCMClientImplTest::OnUnregisterFinished(
    scoped_refptr<RegistrationInfo> registration_info,
    GCMClient::Result result) {
  last_event_ = UNREGISTRATION_COMPLETED;
  last_app_id_ = registration_info->app_id;
  last_result_ = result;
}

void GCMClientImplTest::OnMessagesDeleted(const std::string& app_id) {
  last_event_ = MESSAGES_DELETED;
  last_app_id_ = app_id;
}

void GCMClientImplTest::OnMessageSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& send_error_details) {
  last_event_ = MESSAGE_SEND_ERROR;
  last_app_id_ = app_id;
  last_error_details_ = send_error_details;
}

void GCMClientImplTest::OnSendAcknowledged(const std::string& app_id,
                                           const std::string& message_id) {
  last_event_ = MESSAGE_SEND_ACK;
  last_app_id_ = app_id;
  last_message_id_ = message_id;
}

int64_t GCMClientImplTest::CurrentTime() {
  return clock()->Now().ToInternalValue() / base::Time::kMicrosecondsPerSecond;
}

TEST_F(GCMClientImplTest, LoadingCompleted) {
  EXPECT_EQ(LOADING_COMPLETED, last_event());
  EXPECT_EQ(kDeviceAndroidId, mcs_client()->last_android_id());
  EXPECT_EQ(kDeviceSecurityToken, mcs_client()->last_security_token());

  // Checking freshly loaded CheckinInfo.
  EXPECT_EQ(kDeviceAndroidId, device_checkin_info().android_id);
  EXPECT_EQ(kDeviceSecurityToken, device_checkin_info().secret);
  EXPECT_TRUE(device_checkin_info().last_checkin_accounts.empty());
  EXPECT_TRUE(device_checkin_info().accounts_set);
  EXPECT_TRUE(device_checkin_info().account_tokens.empty());
}

TEST_F(GCMClientImplTest, LoadingBusted) {
  // Close the GCM store.
  gcm_client()->Stop();
  PumpLoopUntilIdle();

  // Mess up the store.
  EXPECT_TRUE(leveldb_chrome::CorruptClosedDBForTesting(gcm_store_path()));

  // Restart GCM client. The store should be reset and the loading should
  // complete successfully.
  reset_last_event();
  BuildGCMClient(base::TimeDelta());
  InitializeGCMClient();
  StartGCMClient();
  ASSERT_NO_FATAL_FAILURE(
      CompleteCheckin(kDeviceAndroidId2, kDeviceSecurityToken2, std::string(),
                      std::map<std::string, std::string>()));

  EXPECT_EQ(LOADING_COMPLETED, last_event());
  EXPECT_EQ(kDeviceAndroidId2, mcs_client()->last_android_id());
  EXPECT_EQ(kDeviceSecurityToken2, mcs_client()->last_security_token());
}

TEST_F(GCMClientImplTest, LoadingWithEmptyDirectory) {
  // Close the GCM store.
  gcm_client()->Stop();
  PumpLoopUntilIdle();

  // Make the store directory empty, to simulate a previous destroy store
  // operation failing to delete the store directory.
  ASSERT_TRUE(base::DeleteFileRecursively(gcm_store_path()));
  ASSERT_TRUE(base::CreateDirectory(gcm_store_path()));

  base::HistogramTester histogram_tester;

  // Restart GCM client. The store should be considered to not exist.
  BuildGCMClient(base::TimeDelta());
  InitializeGCMClient();
  gcm_client()->Start(GCMClient::DELAYED_START);
  PumpLoopUntilIdle();
  histogram_tester.ExpectUniqueSample("GCM.LoadStatus",
                                      13 /* STORE_DOES_NOT_EXIST */, 1);
  // Since the store does not exist, the database should not have been opened.
  histogram_tester.ExpectTotalCount("GCM.Database.Open", 0);
  // Without a store, DELAYED_START loading should only reach INITIALIZED state.
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // The store directory should still exist (and be empty). If not, then the
  // DELAYED_START load has probably reset the store, rather than leaving that
  // to the next IMMEDIATE_START load as expected.
  ASSERT_TRUE(base::DirectoryExists(gcm_store_path()));
  ASSERT_FALSE(
      base::PathExists(gcm_store_path().Append(FILE_PATH_LITERAL("CURRENT"))));

  // IMMEDIATE_START loading should successfully create a new store despite the
  // empty directory.
  reset_last_event();
  StartGCMClient();
  ASSERT_NO_FATAL_FAILURE(
      CompleteCheckin(kDeviceAndroidId2, kDeviceSecurityToken2, std::string(),
                      std::map<std::string, std::string>()));
  EXPECT_EQ(LOADING_COMPLETED, last_event());
  EXPECT_EQ(GCMClientImpl::READY, gcm_client_state());
  EXPECT_EQ(kDeviceAndroidId2, mcs_client()->last_android_id());
  EXPECT_EQ(kDeviceSecurityToken2, mcs_client()->last_security_token());
}

TEST_F(GCMClientImplTest, DestroyStoreWhenNotNeeded) {
  // Close the GCM store.
  gcm_client()->Stop();
  PumpLoopUntilIdle();

  // Restart GCM client. The store is loaded successfully.
  reset_last_event();
  BuildGCMClient(base::TimeDelta());
  InitializeGCMClient();
  gcm_client()->Start(GCMClient::DELAYED_START);
  PumpLoopUntilIdle();

  EXPECT_EQ(GCMClientImpl::LOADED, gcm_client_state());
  EXPECT_TRUE(device_checkin_info().android_id);
  EXPECT_TRUE(device_checkin_info().secret);

  // Fast forward the clock to trigger the store destroying logic.
  FastForwardBy(base::TimeDelta::FromMilliseconds(300000));
  PumpLoopUntilIdle();

  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());
  EXPECT_FALSE(device_checkin_info().android_id);
  EXPECT_FALSE(device_checkin_info().secret);
}

TEST_F(GCMClientImplTest, SerializeAndDeserialize) {
  std::vector<std::string> senders{"sender"};
  auto gcm_info = base::MakeRefCounted<GCMRegistrationInfo>();
  gcm_info->app_id = kExtensionAppId;
  gcm_info->sender_ids = senders;
  gcm_info->last_validated = clock()->Now();

  auto gcm_info_deserialized = base::MakeRefCounted<GCMRegistrationInfo>();
  std::string gcm_registration_id_deserialized;
  {
    std::string serialized_key = gcm_info->GetSerializedKey();
    std::string serialized_value =
        gcm_info->GetSerializedValue(kRegistrationId);

    ASSERT_TRUE(gcm_info_deserialized->Deserialize(
        serialized_key, serialized_value, &gcm_registration_id_deserialized));
  }

  EXPECT_EQ(gcm_info->app_id, gcm_info_deserialized->app_id);
  EXPECT_EQ(gcm_info->sender_ids, gcm_info_deserialized->sender_ids);
  EXPECT_EQ(gcm_info->last_validated, gcm_info_deserialized->last_validated);
  EXPECT_EQ(kRegistrationId, gcm_registration_id_deserialized);

  auto instance_id_info = base::MakeRefCounted<InstanceIDTokenInfo>();
  instance_id_info->app_id = kExtensionAppId;
  instance_id_info->last_validated = clock()->Now();
  instance_id_info->authorized_entity = "different_sender";
  instance_id_info->scope = "scope";

  auto instance_id_info_deserialized =
      base::MakeRefCounted<InstanceIDTokenInfo>();
  std::string instance_id_registration_id_deserialized;
  {
    std::string serialized_key = instance_id_info->GetSerializedKey();
    std::string serialized_value =
        instance_id_info->GetSerializedValue(kRegistrationId);

    ASSERT_TRUE(instance_id_info_deserialized->Deserialize(
        serialized_key, serialized_value,
        &instance_id_registration_id_deserialized));
  }

  EXPECT_EQ(instance_id_info->app_id, instance_id_info_deserialized->app_id);
  EXPECT_EQ(instance_id_info->last_validated,
            instance_id_info_deserialized->last_validated);
  EXPECT_EQ(instance_id_info->authorized_entity,
            instance_id_info_deserialized->authorized_entity);
  EXPECT_EQ(instance_id_info->scope, instance_id_info_deserialized->scope);
  EXPECT_EQ(kRegistrationId, instance_id_registration_id_deserialized);
}

TEST_F(GCMClientImplTest, RegisterApp) {
  EXPECT_FALSE(ExistsRegistration(kExtensionAppId));

  std::vector<std::string> senders;
  senders.push_back("sender");
  Register(kExtensionAppId, senders);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("reg_id"));

  EXPECT_EQ(REGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ("reg_id", last_registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  EXPECT_TRUE(ExistsRegistration(kExtensionAppId));
}

TEST_F(GCMClientImplTest, RegisterAppFromCache) {
  EXPECT_FALSE(ExistsRegistration(kExtensionAppId));

  std::vector<std::string> senders;
  senders.push_back("sender");
  Register(kExtensionAppId, senders);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("reg_id"));
  EXPECT_TRUE(ExistsRegistration(kExtensionAppId));

  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ("reg_id", last_registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  EXPECT_EQ(REGISTRATION_COMPLETED, last_event());

  // Recreate GCMClient in order to load from the persistent store.
  BuildGCMClient(base::TimeDelta());
  InitializeGCMClient();
  StartGCMClient();

  EXPECT_TRUE(ExistsRegistration(kExtensionAppId));
}

TEST_F(GCMClientImplTest, RegisterPreviousSenderAgain) {
  EXPECT_FALSE(ExistsRegistration(kExtensionAppId));

  // Register a sender.
  std::vector<std::string> senders;
  senders.push_back("sender");
  Register(kExtensionAppId, senders);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("reg_id"));

  EXPECT_EQ(REGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ("reg_id", last_registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  EXPECT_TRUE(ExistsRegistration(kExtensionAppId));

  reset_last_event();

  // Register a different sender. Different registration ID from previous one
  // should be returned.
  std::vector<std::string> senders2;
  senders2.push_back("sender2");
  Register(kExtensionAppId, senders2);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("reg_id2"));

  EXPECT_EQ(REGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ("reg_id2", last_registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  EXPECT_TRUE(ExistsRegistration(kExtensionAppId));

  reset_last_event();

  // Register the 1st sender again. Different registration ID from previous one
  // should be returned.
  std::vector<std::string> senders3;
  senders3.push_back("sender");
  Register(kExtensionAppId, senders3);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("reg_id"));

  EXPECT_EQ(REGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ("reg_id", last_registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  EXPECT_TRUE(ExistsRegistration(kExtensionAppId));
}

TEST_F(GCMClientImplTest, DISABLED_RegisterAgainWhenTokenIsFresh) {
  // Register a sender.
  std::vector<std::string> senders;
  senders.push_back("sender");
  Register(kExtensionAppId, senders);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("reg_id"));
  EXPECT_EQ(REGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ("reg_id", last_registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  EXPECT_TRUE(ExistsRegistration(kExtensionAppId));

  reset_last_event();

  // Advance time by (kTestTokenInvalidationPeriod)/2
  clock()->Advance(base::TimeDelta::FromDays(kTestTokenInvalidationPeriod / 2));

  // Register the same sender again. The same registration ID as the
  // previous one should be returned, and we should *not* send a
  // registration request to the GCM server.
  Register(kExtensionAppId, senders);
  PumpLoopUntilIdle();

  EXPECT_EQ(REGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ("reg_id", last_registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  EXPECT_TRUE(ExistsRegistration(kExtensionAppId));
}

TEST_F(GCMClientImplTest, RegisterAgainWhenTokenIsStale) {
  // Register a sender.
  std::vector<std::string> senders;
  senders.push_back("sender");
  Register(kExtensionAppId, senders);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("reg_id"));

  EXPECT_EQ(REGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ("reg_id", last_registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  EXPECT_TRUE(ExistsRegistration(kExtensionAppId));

  reset_last_event();

  // Advance time by kTestTokenInvalidationPeriod
  clock()->Advance(base::TimeDelta::FromDays(kTestTokenInvalidationPeriod));

  // Register the same sender again. Different registration ID from the
  // previous one should be returned.
  Register(kExtensionAppId, senders);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("reg_id2"));

  EXPECT_EQ(REGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ("reg_id2", last_registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  EXPECT_TRUE(ExistsRegistration(kExtensionAppId));
}

TEST_F(GCMClientImplTest, UnregisterApp) {
  EXPECT_FALSE(ExistsRegistration(kExtensionAppId));

  std::vector<std::string> senders;
  senders.push_back("sender");
  Register(kExtensionAppId, senders);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("reg_id"));
  EXPECT_TRUE(ExistsRegistration(kExtensionAppId));

  Unregister(kExtensionAppId);
  ASSERT_NO_FATAL_FAILURE(CompleteUnregistration(kExtensionAppId));

  EXPECT_EQ(UNREGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  EXPECT_FALSE(ExistsRegistration(kExtensionAppId));
}

// Tests that stopping the GCMClient also deletes pending registration requests.
// This is tested by checking that url fetcher contained in the request was
// deleted.
TEST_F(GCMClientImplTest, DeletePendingRequestsWhenStopping) {
  std::vector<std::string> senders;
  senders.push_back("sender");
  Register(kExtensionAppId, senders);

  gcm_client()->Stop();
  PumpLoopUntilIdle();
  EXPECT_EQ(0, url_loader_factory()->NumPending());
}

TEST_F(GCMClientImplTest, DispatchDownstreamMessage) {
  // Register to receive messages from kSender and kSender2 only.
  std::vector<std::string> senders;
  senders.push_back(kSender);
  senders.push_back(kSender2);
  AddRegistration(kExtensionAppId, senders, "reg_id");

  std::map<std::string, std::string> expected_data;
  expected_data["message_type"] = "gcm";
  expected_data["key"] = "value";
  expected_data["key2"] = "value2";

  // Message for kSender will be received.
  MCSMessage message(BuildDownstreamMessage(
      kSender, kExtensionAppId, std::string() /* subtype */, expected_data,
      std::string() /* raw_data */));
  EXPECT_TRUE(message.IsValid());
  ReceiveMessageFromMCS(message);

  expected_data.erase(expected_data.find("message_type"));
  EXPECT_EQ(MESSAGE_RECEIVED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ(expected_data.size(), last_message().data.size());
  EXPECT_EQ(expected_data, last_message().data);
  EXPECT_EQ(kSender, last_message().sender_id);

  reset_last_event();

  // Message for kSender2 will be received.
  MCSMessage message2(BuildDownstreamMessage(
      kSender2, kExtensionAppId, std::string() /* subtype */, expected_data,
      std::string() /* raw_data */));
  EXPECT_TRUE(message2.IsValid());
  ReceiveMessageFromMCS(message2);

  EXPECT_EQ(MESSAGE_RECEIVED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ(expected_data.size(), last_message().data.size());
  EXPECT_EQ(expected_data, last_message().data);
  EXPECT_EQ(kSender2, last_message().sender_id);
}

TEST_F(GCMClientImplTest, DispatchDownstreamMessageRawData) {
  std::vector<std::string> senders(1, kSender);
  AddRegistration(kExtensionAppId, senders, "reg_id");

  std::map<std::string, std::string> expected_data;

  MCSMessage message(BuildDownstreamMessage(kSender, kExtensionAppId,
                                            std::string() /* subtype */,
                                            expected_data, kRawData));
  EXPECT_TRUE(message.IsValid());
  ReceiveMessageFromMCS(message);

  EXPECT_EQ(MESSAGE_RECEIVED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ(expected_data.size(), last_message().data.size());
  EXPECT_EQ(kSender, last_message().sender_id);
  EXPECT_EQ(kRawData, last_message().raw_data);
}

TEST_F(GCMClientImplTest, DISABLED_DispatchDownstreamMessageSendError) {
  std::map<std::string, std::string> expected_data = {
      {"message_type", "send_error"}, {"error_details", "some details"}};

  MCSMessage message(BuildDownstreamMessage(
      kSender, kExtensionAppId, std::string() /* subtype */, expected_data,
      std::string() /* raw_data */));
  EXPECT_TRUE(message.IsValid());
  ReceiveMessageFromMCS(message);

  EXPECT_EQ(MESSAGE_SEND_ERROR, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ(kMessageId, last_error_details().message_id);
  EXPECT_EQ(1UL, last_error_details().additional_data.size());
  auto iter = last_error_details().additional_data.find("error_details");
  EXPECT_TRUE(iter != last_error_details().additional_data.end());
  EXPECT_EQ("some details", iter->second);
}

TEST_F(GCMClientImplTest, DispatchDownstreamMessgaesDeleted) {
  std::map<std::string, std::string> expected_data;
  expected_data["message_type"] = "deleted_messages";
  MCSMessage message(BuildDownstreamMessage(
      kSender, kExtensionAppId, std::string() /* subtype */, expected_data,
      std::string() /* raw_data */));
  EXPECT_TRUE(message.IsValid());
  ReceiveMessageFromMCS(message);

  EXPECT_EQ(MESSAGES_DELETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
}

TEST_F(GCMClientImplTest, SendMessage) {
  OutgoingMessage message;
  message.id = "007";
  message.time_to_live = 500;
  message.data["key"] = "value";
  gcm_client()->Send(kExtensionAppId, kSender, message);

  EXPECT_EQ(kDataMessageStanzaTag, mcs_client()->last_message_tag());
  EXPECT_EQ(kExtensionAppId,
            mcs_client()->last_data_message_stanza().category());
  EXPECT_EQ(kSender, mcs_client()->last_data_message_stanza().to());
  EXPECT_EQ(500, mcs_client()->last_data_message_stanza().ttl());
  EXPECT_EQ(CurrentTime(), mcs_client()->last_data_message_stanza().sent());
  EXPECT_EQ("007", mcs_client()->last_data_message_stanza().id());
  EXPECT_EQ("gcm@chrome.com", mcs_client()->last_data_message_stanza().from());
  EXPECT_EQ(kSender, mcs_client()->last_data_message_stanza().to());
  EXPECT_EQ("key", mcs_client()->last_data_message_stanza().app_data(0).key());
  EXPECT_EQ("value",
            mcs_client()->last_data_message_stanza().app_data(0).value());
}

TEST_F(GCMClientImplTest, SendMessageAcknowledged) {
  ReceiveOnMessageSentToMCS(kExtensionAppId, "007", MCSClient::SENT);
  EXPECT_EQ(MESSAGE_SEND_ACK, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ("007", last_message_id());
}

class GCMClientImplCheckinTest : public GCMClientImplTest {
 public:
  GCMClientImplCheckinTest();
  ~GCMClientImplCheckinTest() override;

  void SetUp() override;
};

GCMClientImplCheckinTest::GCMClientImplCheckinTest() {
}

GCMClientImplCheckinTest::~GCMClientImplCheckinTest() {
}

void GCMClientImplCheckinTest::SetUp() {
  testing::Test::SetUp();
  // Creating unique temp directory that will be used by GCMStore shared between
  // GCM Client and G-services settings.
  ASSERT_TRUE(CreateUniqueTempDir());
  // Time will be advancing one hour every time it is checked.
  BuildGCMClient(base::TimeDelta::FromSeconds(kSettingsCheckinInterval));
  InitializeGCMClient();
  StartGCMClient();
}

TEST_F(GCMClientImplCheckinTest, GServicesSettingsAfterInitialCheckin) {
  std::map<std::string, std::string> settings;
  settings["checkin_interval"] = base::NumberToString(kSettingsCheckinInterval);
  settings["checkin_url"] = "http://alternative.url/checkin";
  settings["gcm_hostname"] = "alternative.gcm.host";
  settings["gcm_secure_port"] = "7777";
  settings["gcm_registration_url"] = "http://alternative.url/registration";
  ASSERT_NO_FATAL_FAILURE(
      CompleteCheckin(kDeviceAndroidId, kDeviceSecurityToken,
                      GServicesSettings::CalculateDigest(settings), settings));
  EXPECT_EQ(base::TimeDelta::FromSeconds(kSettingsCheckinInterval),
            gservices_settings().GetCheckinInterval());
  EXPECT_EQ(GURL("http://alternative.url/checkin"),
            gservices_settings().GetCheckinURL());
  EXPECT_EQ(GURL("http://alternative.url/registration"),
            gservices_settings().GetRegistrationURL());
  EXPECT_EQ(GURL("https://alternative.gcm.host:7777"),
            gservices_settings().GetMCSMainEndpoint());
  EXPECT_EQ(GURL("https://alternative.gcm.host:443"),
            gservices_settings().GetMCSFallbackEndpoint());
}

// This test only checks that periodic checkin happens.
TEST_F(GCMClientImplCheckinTest, PeriodicCheckin) {
  std::map<std::string, std::string> settings;
  settings["checkin_interval"] = base::NumberToString(kSettingsCheckinInterval);
  settings["checkin_url"] = "http://alternative.url/checkin";
  settings["gcm_hostname"] = "alternative.gcm.host";
  settings["gcm_secure_port"] = "7777";
  settings["gcm_registration_url"] = "http://alternative.url/registration";
  ASSERT_NO_FATAL_FAILURE(
      CompleteCheckin(kDeviceAndroidId, kDeviceSecurityToken,
                      GServicesSettings::CalculateDigest(settings), settings));

  EXPECT_EQ(2, clock()->call_count());

  PumpLoopUntilIdle();
  ASSERT_NO_FATAL_FAILURE(
      CompleteCheckin(kDeviceAndroidId, kDeviceSecurityToken,
                      GServicesSettings::CalculateDigest(settings), settings));
}

TEST_F(GCMClientImplCheckinTest, LoadGSettingsFromStore) {
  std::map<std::string, std::string> settings;
  settings["checkin_interval"] = base::NumberToString(kSettingsCheckinInterval);
  settings["checkin_url"] = "http://alternative.url/checkin";
  settings["gcm_hostname"] = "alternative.gcm.host";
  settings["gcm_secure_port"] = "7777";
  settings["gcm_registration_url"] = "http://alternative.url/registration";
  ASSERT_NO_FATAL_FAILURE(
      CompleteCheckin(kDeviceAndroidId, kDeviceSecurityToken,
                      GServicesSettings::CalculateDigest(settings), settings));

  BuildGCMClient(base::TimeDelta());
  InitializeGCMClient();
  StartGCMClient();

  EXPECT_EQ(base::TimeDelta::FromSeconds(kSettingsCheckinInterval),
            gservices_settings().GetCheckinInterval());
  EXPECT_EQ(GURL("http://alternative.url/checkin"),
            gservices_settings().GetCheckinURL());
  EXPECT_EQ(GURL("http://alternative.url/registration"),
            gservices_settings().GetRegistrationURL());
  EXPECT_EQ(GURL("https://alternative.gcm.host:7777"),
            gservices_settings().GetMCSMainEndpoint());
  EXPECT_EQ(GURL("https://alternative.gcm.host:443"),
            gservices_settings().GetMCSFallbackEndpoint());
}

// This test only checks that periodic checkin happens.
TEST_F(GCMClientImplCheckinTest, CheckinWithAccounts) {
  std::map<std::string, std::string> settings;
  settings["checkin_interval"] = base::NumberToString(kSettingsCheckinInterval);
  settings["checkin_url"] = "http://alternative.url/checkin";
  settings["gcm_hostname"] = "alternative.gcm.host";
  settings["gcm_secure_port"] = "7777";
  settings["gcm_registration_url"] = "http://alternative.url/registration";
  ASSERT_NO_FATAL_FAILURE(
      CompleteCheckin(kDeviceAndroidId, kDeviceSecurityToken,
                      GServicesSettings::CalculateDigest(settings), settings));

  std::vector<GCMClient::AccountTokenInfo> account_tokens;
  account_tokens.push_back(MakeAccountToken("test_user1@gmail.com", "token1"));
  account_tokens.push_back(MakeAccountToken("test_user2@gmail.com", "token2"));
  gcm_client()->SetAccountTokens(account_tokens);

  EXPECT_TRUE(device_checkin_info().last_checkin_accounts.empty());
  EXPECT_TRUE(device_checkin_info().accounts_set);
  EXPECT_EQ(MakeEmailToTokenMap(account_tokens),
            device_checkin_info().account_tokens);

  PumpLoopUntilIdle();
  ASSERT_NO_FATAL_FAILURE(
      CompleteCheckin(kDeviceAndroidId, kDeviceSecurityToken,
                      GServicesSettings::CalculateDigest(settings), settings));

  std::set<std::string> accounts;
  accounts.insert("test_user1@gmail.com");
  accounts.insert("test_user2@gmail.com");
  EXPECT_EQ(accounts, device_checkin_info().last_checkin_accounts);
  EXPECT_TRUE(device_checkin_info().accounts_set);
  EXPECT_EQ(MakeEmailToTokenMap(account_tokens),
            device_checkin_info().account_tokens);
}

// This test only checks that periodic checkin happens.
TEST_F(GCMClientImplCheckinTest, CheckinWhenAccountRemoved) {
  std::map<std::string, std::string> settings;
  settings["checkin_interval"] = base::NumberToString(kSettingsCheckinInterval);
  settings["checkin_url"] = "http://alternative.url/checkin";
  settings["gcm_hostname"] = "alternative.gcm.host";
  settings["gcm_secure_port"] = "7777";
  settings["gcm_registration_url"] = "http://alternative.url/registration";
  ASSERT_NO_FATAL_FAILURE(
      CompleteCheckin(kDeviceAndroidId, kDeviceSecurityToken,
                      GServicesSettings::CalculateDigest(settings), settings));

  std::vector<GCMClient::AccountTokenInfo> account_tokens;
  account_tokens.push_back(MakeAccountToken("test_user1@gmail.com", "token1"));
  account_tokens.push_back(MakeAccountToken("test_user2@gmail.com", "token2"));
  gcm_client()->SetAccountTokens(account_tokens);
  PumpLoopUntilIdle();
  ASSERT_NO_FATAL_FAILURE(
      CompleteCheckin(kDeviceAndroidId, kDeviceSecurityToken,
                      GServicesSettings::CalculateDigest(settings), settings));

  EXPECT_EQ(2UL, device_checkin_info().last_checkin_accounts.size());
  EXPECT_TRUE(device_checkin_info().accounts_set);
  EXPECT_EQ(MakeEmailToTokenMap(account_tokens),
            device_checkin_info().account_tokens);

  account_tokens.erase(account_tokens.begin() + 1);
  gcm_client()->SetAccountTokens(account_tokens);

  PumpLoopUntilIdle();
  ASSERT_NO_FATAL_FAILURE(
      CompleteCheckin(kDeviceAndroidId, kDeviceSecurityToken,
                      GServicesSettings::CalculateDigest(settings), settings));

  std::set<std::string> accounts;
  accounts.insert("test_user1@gmail.com");
  EXPECT_EQ(accounts, device_checkin_info().last_checkin_accounts);
  EXPECT_TRUE(device_checkin_info().accounts_set);
  EXPECT_EQ(MakeEmailToTokenMap(account_tokens),
            device_checkin_info().account_tokens);
}

// This test only checks that periodic checkin happens.
TEST_F(GCMClientImplCheckinTest, CheckinWhenAccountReplaced) {
  std::map<std::string, std::string> settings;
  settings["checkin_interval"] = base::NumberToString(kSettingsCheckinInterval);
  settings["checkin_url"] = "http://alternative.url/checkin";
  settings["gcm_hostname"] = "alternative.gcm.host";
  settings["gcm_secure_port"] = "7777";
  settings["gcm_registration_url"] = "http://alternative.url/registration";
  ASSERT_NO_FATAL_FAILURE(
      CompleteCheckin(kDeviceAndroidId, kDeviceSecurityToken,
                      GServicesSettings::CalculateDigest(settings), settings));

  std::vector<GCMClient::AccountTokenInfo> account_tokens;
  account_tokens.push_back(MakeAccountToken("test_user1@gmail.com", "token1"));
  gcm_client()->SetAccountTokens(account_tokens);

  PumpLoopUntilIdle();
  ASSERT_NO_FATAL_FAILURE(
      CompleteCheckin(kDeviceAndroidId, kDeviceSecurityToken,
                      GServicesSettings::CalculateDigest(settings), settings));

  std::set<std::string> accounts;
  accounts.insert("test_user1@gmail.com");
  EXPECT_EQ(accounts, device_checkin_info().last_checkin_accounts);

  // This should trigger another checkin, because the list of accounts is
  // different.
  account_tokens.clear();
  account_tokens.push_back(MakeAccountToken("test_user2@gmail.com", "token2"));
  gcm_client()->SetAccountTokens(account_tokens);

  PumpLoopUntilIdle();
  ASSERT_NO_FATAL_FAILURE(
      CompleteCheckin(kDeviceAndroidId, kDeviceSecurityToken,
                      GServicesSettings::CalculateDigest(settings), settings));

  accounts.clear();
  accounts.insert("test_user2@gmail.com");
  EXPECT_EQ(accounts, device_checkin_info().last_checkin_accounts);
  EXPECT_TRUE(device_checkin_info().accounts_set);
  EXPECT_EQ(MakeEmailToTokenMap(account_tokens),
            device_checkin_info().account_tokens);
}

TEST_F(GCMClientImplCheckinTest, ResetStoreWhenCheckinRejected) {
  base::HistogramTester histogram_tester;
  std::map<std::string, std::string> settings;
  ASSERT_NO_FATAL_FAILURE(FailCheckin(net::HTTP_UNAUTHORIZED));
  PumpLoopUntilIdle();

  // Store should have been destroyed. Restart client and verify the initial
  // checkin response is persisted.
  BuildGCMClient(base::TimeDelta());
  InitializeGCMClient();
  StartGCMClient();
  ASSERT_NO_FATAL_FAILURE(
      CompleteCheckin(kDeviceAndroidId2, kDeviceSecurityToken2,
                      GServicesSettings::CalculateDigest(settings), settings));

  EXPECT_EQ(LOADING_COMPLETED, last_event());
  EXPECT_EQ(kDeviceAndroidId2, mcs_client()->last_android_id());
  EXPECT_EQ(kDeviceSecurityToken2, mcs_client()->last_security_token());
}

class GCMClientImplStartAndStopTest : public GCMClientImplTest {
 public:
  GCMClientImplStartAndStopTest();
  ~GCMClientImplStartAndStopTest() override;

  void SetUp() override;

  void DefaultCompleteCheckin();
};

GCMClientImplStartAndStopTest::GCMClientImplStartAndStopTest() {
}

GCMClientImplStartAndStopTest::~GCMClientImplStartAndStopTest() {
}

void GCMClientImplStartAndStopTest::SetUp() {
  testing::Test::SetUp();
  ASSERT_TRUE(CreateUniqueTempDir());
  BuildGCMClient(base::TimeDelta());
  InitializeGCMClient();
}

void GCMClientImplStartAndStopTest::DefaultCompleteCheckin() {
  ASSERT_NO_FATAL_FAILURE(
      CompleteCheckin(kDeviceAndroidId, kDeviceSecurityToken, std::string(),
                      std::map<std::string, std::string>()));
  PumpLoopUntilIdle();
}

TEST_F(GCMClientImplStartAndStopTest, DISABLED_StartStopAndRestart) {
  // GCMClientImpl should be in INITIALIZED state at first.
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Delay start the GCM.
  gcm_client()->Start(GCMClient::DELAYED_START);
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Stop the GCM.
  gcm_client()->Stop();
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Restart the GCM without delay.
  gcm_client()->Start(GCMClient::IMMEDIATE_START);
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIAL_DEVICE_CHECKIN, gcm_client_state());
}

TEST_F(GCMClientImplStartAndStopTest, DelayedStartAndStopImmediately) {
  // GCMClientImpl should be in INITIALIZED state at first.
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Delay start the GCM and then stop it immediately.
  gcm_client()->Start(GCMClient::DELAYED_START);
  gcm_client()->Stop();
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());
}

TEST_F(GCMClientImplStartAndStopTest, ImmediateStartAndStopImmediately) {
  // GCMClientImpl should be in INITIALIZED state at first.
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Start the GCM and then stop it immediately.
  gcm_client()->Start(GCMClient::IMMEDIATE_START);
  gcm_client()->Stop();
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());
}

TEST_F(GCMClientImplStartAndStopTest, DelayedStartStopAndRestart) {
  // GCMClientImpl should be in INITIALIZED state at first.
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Delay start the GCM and then stop and restart it immediately.
  gcm_client()->Start(GCMClient::DELAYED_START);
  gcm_client()->Stop();
  gcm_client()->Start(GCMClient::DELAYED_START);
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());
}

TEST_F(GCMClientImplStartAndStopTest, ImmediateStartStopAndRestart) {
  // GCMClientImpl should be in INITIALIZED state at first.
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Start the GCM and then stop and restart it immediately.
  gcm_client()->Start(GCMClient::IMMEDIATE_START);
  gcm_client()->Stop();
  gcm_client()->Start(GCMClient::IMMEDIATE_START);
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIAL_DEVICE_CHECKIN, gcm_client_state());
}

TEST_F(GCMClientImplStartAndStopTest, ImmediateStartAndThenImmediateStart) {
  // GCMClientImpl should be in INITIALIZED state at first.
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Start the GCM immediately and complete the checkin.
  gcm_client()->Start(GCMClient::IMMEDIATE_START);
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIAL_DEVICE_CHECKIN, gcm_client_state());
  ASSERT_NO_FATAL_FAILURE(DefaultCompleteCheckin());
  EXPECT_EQ(GCMClientImpl::READY, gcm_client_state());

  // Stop the GCM.
  gcm_client()->Stop();
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Start the GCM immediately. GCMClientImpl should be in READY state.
  BuildGCMClient(base::TimeDelta());
  InitializeGCMClient();
  gcm_client()->Start(GCMClient::IMMEDIATE_START);
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::READY, gcm_client_state());
}

TEST_F(GCMClientImplStartAndStopTest, ImmediateStartAndThenDelayStart) {
  // GCMClientImpl should be in INITIALIZED state at first.
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Start the GCM immediately and complete the checkin.
  gcm_client()->Start(GCMClient::IMMEDIATE_START);
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIAL_DEVICE_CHECKIN, gcm_client_state());
  ASSERT_NO_FATAL_FAILURE(DefaultCompleteCheckin());
  EXPECT_EQ(GCMClientImpl::READY, gcm_client_state());

  // Stop the GCM.
  gcm_client()->Stop();
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Delay start the GCM. GCMClientImpl should be in LOADED state.
  BuildGCMClient(base::TimeDelta());
  InitializeGCMClient();
  gcm_client()->Start(GCMClient::DELAYED_START);
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::LOADED, gcm_client_state());
}

TEST_F(GCMClientImplStartAndStopTest, DISABLED_DelayedStartRace) {
  // GCMClientImpl should be in INITIALIZED state at first.
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Delay start the GCM, then start it immediately while it's still loading.
  gcm_client()->Start(GCMClient::DELAYED_START);
  gcm_client()->Start(GCMClient::IMMEDIATE_START);
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIAL_DEVICE_CHECKIN, gcm_client_state());
  ASSERT_NO_FATAL_FAILURE(DefaultCompleteCheckin());
  EXPECT_EQ(GCMClientImpl::READY, gcm_client_state());
}

TEST_F(GCMClientImplStartAndStopTest, DelayedStart) {
  // GCMClientImpl should be in INITIALIZED state at first.
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Delay start the GCM. The store will not be loaded and GCMClientImpl should
  // still be in INITIALIZED state.
  gcm_client()->Start(GCMClient::DELAYED_START);
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Start the GCM immediately and complete the checkin.
  gcm_client()->Start(GCMClient::IMMEDIATE_START);
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIAL_DEVICE_CHECKIN, gcm_client_state());
  ASSERT_NO_FATAL_FAILURE(DefaultCompleteCheckin());
  EXPECT_EQ(GCMClientImpl::READY, gcm_client_state());

  // Registration.
  std::vector<std::string> senders;
  senders.push_back("sender");
  Register(kExtensionAppId, senders);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("reg_id"));
  EXPECT_EQ(GCMClientImpl::READY, gcm_client_state());

  // Stop the GCM.
  gcm_client()->Stop();
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Delay start the GCM. GCM is indeed started without delay because the
  // registration record has been found.
  BuildGCMClient(base::TimeDelta());
  InitializeGCMClient();
  gcm_client()->Start(GCMClient::DELAYED_START);
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::READY, gcm_client_state());
}

// Test for known account mappings and last token fetching time being passed
// to OnGCMReady.
TEST_F(GCMClientImplStartAndStopTest, OnGCMReadyAccountsAndTokenFetchingTime) {
  // Start the GCM and wait until it is ready.
  gcm_client()->Start(GCMClient::IMMEDIATE_START);
  PumpLoopUntilIdle();
  ASSERT_NO_FATAL_FAILURE(DefaultCompleteCheckin());

  base::Time expected_time = base::Time::Now();
  gcm_client()->SetLastTokenFetchTime(expected_time);
  AccountMapping expected_mapping;
  expected_mapping.account_id = CoreAccountId("accId");
  expected_mapping.email = "email@gmail.com";
  expected_mapping.status = AccountMapping::MAPPED;
  expected_mapping.status_change_timestamp = expected_time;
  gcm_client()->UpdateAccountMapping(expected_mapping);
  PumpLoopUntilIdle();

  // Stop the GCM.
  gcm_client()->Stop();
  PumpLoopUntilIdle();

  // Restart the GCM.
  gcm_client()->Start(GCMClient::IMMEDIATE_START);
  PumpLoopUntilIdle();

  EXPECT_EQ(LOADING_COMPLETED, last_event());
  EXPECT_EQ(expected_time, last_token_fetch_time());
  ASSERT_EQ(1UL, last_account_mappings().size());
  const AccountMapping& actual_mapping = last_account_mappings()[0];
  EXPECT_EQ(expected_mapping.account_id, actual_mapping.account_id);
  EXPECT_EQ(expected_mapping.email, actual_mapping.email);
  EXPECT_EQ(expected_mapping.status, actual_mapping.status);
  EXPECT_EQ(expected_mapping.status_change_timestamp,
            actual_mapping.status_change_timestamp);
}


class GCMClientInstanceIDTest : public GCMClientImplTest {
 public:
  GCMClientInstanceIDTest();
  ~GCMClientInstanceIDTest() override;

  void AddInstanceID(const std::string& app_id,
                     const std::string& instance_id);
  void RemoveInstanceID(const std::string& app_id);
  void GetToken(const std::string& app_id,
                const std::string& authorized_entity,
                const std::string& scope);
  void DeleteToken(const std::string& app_id,
                   const std::string& authorized_entity,
                   const std::string& scope);
  void CompleteDeleteToken();
  bool ExistsToken(const std::string& app_id,
                   const std::string& authorized_entity,
                   const std::string& scope) const;
};

GCMClientInstanceIDTest::GCMClientInstanceIDTest() {
}

GCMClientInstanceIDTest::~GCMClientInstanceIDTest() {
}

void GCMClientInstanceIDTest::AddInstanceID(const std::string& app_id,
                                            const std::string& instance_id) {
  gcm_client()->AddInstanceIDData(app_id, instance_id, "123");
}

void GCMClientInstanceIDTest::RemoveInstanceID(const std::string& app_id) {
  gcm_client()->RemoveInstanceIDData(app_id);
}

void GCMClientInstanceIDTest::GetToken(const std::string& app_id,
                                       const std::string& authorized_entity,
                                       const std::string& scope) {
  auto instance_id_info = base::MakeRefCounted<InstanceIDTokenInfo>();
  instance_id_info->app_id = app_id;
  instance_id_info->authorized_entity = authorized_entity;
  instance_id_info->scope = scope;
  gcm_client()->Register(std::move(instance_id_info));
}

void GCMClientInstanceIDTest::DeleteToken(const std::string& app_id,
                                          const std::string& authorized_entity,
                                          const std::string& scope) {
  auto instance_id_info = base::MakeRefCounted<InstanceIDTokenInfo>();
  instance_id_info->app_id = app_id;
  instance_id_info->authorized_entity = authorized_entity;
  instance_id_info->scope = scope;
  gcm_client()->Unregister(std::move(instance_id_info));
}

void GCMClientInstanceIDTest::CompleteDeleteToken() {
  std::string response(kDeleteTokenResponse);

  EXPECT_TRUE(url_loader_factory()->SimulateResponseForPendingRequest(
      GURL(kRegisterUrl), network::URLLoaderCompletionStatus(net::OK),
      network::CreateURLResponseHead(net::HTTP_OK), response));

  // Give a chance for GCMStoreImpl::Backend to finish persisting data.
  PumpLoopUntilIdle();
}

bool GCMClientInstanceIDTest::ExistsToken(const std::string& app_id,
                                          const std::string& authorized_entity,
                                          const std::string& scope) const {
  auto instance_id_info = base::MakeRefCounted<InstanceIDTokenInfo>();
  instance_id_info->app_id = app_id;
  instance_id_info->authorized_entity = authorized_entity;
  instance_id_info->scope = scope;
  return gcm_client()->registrations_.count(std::move(instance_id_info)) > 0;
}

TEST_F(GCMClientInstanceIDTest, GetToken) {
  AddInstanceID(kExtensionAppId, kInstanceID);

  // Get a token.
  EXPECT_FALSE(ExistsToken(kExtensionAppId, kSender, kScope));
  GetToken(kExtensionAppId, kSender, kScope);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("token1"));

  EXPECT_EQ(REGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ("token1", last_registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  EXPECT_TRUE(ExistsToken(kExtensionAppId, kSender, kScope));

  // Get another token.
  EXPECT_FALSE(ExistsToken(kExtensionAppId, kSender2, kScope));
  GetToken(kExtensionAppId, kSender2, kScope);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("token2"));

  EXPECT_EQ(REGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ("token2", last_registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  EXPECT_TRUE(ExistsToken(kExtensionAppId, kSender2, kScope));
  // The 1st token still exists.
  EXPECT_TRUE(ExistsToken(kExtensionAppId, kSender, kScope));
}

// Most tests in this file use kExtensionAppId which is special-cased by
// InstanceIDUsesSubtypeForAppId in gcm_client_impl.cc. This test uses
// kSubtypeAppId to cover the alternate case.
TEST_F(GCMClientInstanceIDTest, GetTokenWithSubtype) {
  ASSERT_EQ(GCMClientImpl::READY, gcm_client_state());

  AddInstanceID(kSubtypeAppId, kInstanceID);

  EXPECT_FALSE(ExistsToken(kSubtypeAppId, kSender, kScope));

  // Get a token.
  GetToken(kSubtypeAppId, kSender, kScope);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("token1"));
  EXPECT_EQ(REGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kSubtypeAppId, last_app_id());
  EXPECT_EQ("token1", last_registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  EXPECT_TRUE(ExistsToken(kSubtypeAppId, kSender, kScope));

  // Delete the token.
  DeleteToken(kSubtypeAppId, kSender, kScope);
  ASSERT_NO_FATAL_FAILURE(CompleteDeleteToken());
  EXPECT_FALSE(ExistsToken(kSubtypeAppId, kSender, kScope));
}

TEST_F(GCMClientInstanceIDTest, DeleteInvalidToken) {
  AddInstanceID(kExtensionAppId, kInstanceID);

  // Delete an invalid token.
  DeleteToken(kExtensionAppId, "Foo@#$", kScope);
  PumpLoopUntilIdle();

  EXPECT_EQ(UNREGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ(GCMClient::INVALID_PARAMETER, last_result());

  reset_last_event();

  // Delete a non-existing token.
  DeleteToken(kExtensionAppId, kSender, kScope);
  PumpLoopUntilIdle();

  EXPECT_EQ(UNREGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ(GCMClient::INVALID_PARAMETER, last_result());
}

TEST_F(GCMClientInstanceIDTest, DeleteSingleToken) {
  AddInstanceID(kExtensionAppId, kInstanceID);

  // Get a token.
  EXPECT_FALSE(ExistsToken(kExtensionAppId, kSender, kScope));
  GetToken(kExtensionAppId, kSender, kScope);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("token1"));

  EXPECT_EQ(REGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ("token1", last_registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  EXPECT_TRUE(ExistsToken(kExtensionAppId, kSender, kScope));

  reset_last_event();

  // Get another token.
  EXPECT_FALSE(ExistsToken(kExtensionAppId, kSender2, kScope));
  GetToken(kExtensionAppId, kSender2, kScope);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("token2"));

  EXPECT_EQ(REGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ("token2", last_registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  EXPECT_TRUE(ExistsToken(kExtensionAppId, kSender2, kScope));
  // The 1st token still exists.
  EXPECT_TRUE(ExistsToken(kExtensionAppId, kSender, kScope));

  reset_last_event();

  // Delete the 2nd token.
  DeleteToken(kExtensionAppId, kSender2, kScope);
  ASSERT_NO_FATAL_FAILURE(CompleteDeleteToken());

  EXPECT_EQ(UNREGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  // The 2nd token is gone while the 1st token still exists.
  EXPECT_TRUE(ExistsToken(kExtensionAppId, kSender, kScope));
  EXPECT_FALSE(ExistsToken(kExtensionAppId, kSender2, kScope));

  reset_last_event();

  // Delete the 1st token.
  DeleteToken(kExtensionAppId, kSender, kScope);
  ASSERT_NO_FATAL_FAILURE(CompleteDeleteToken());

  EXPECT_EQ(UNREGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  // Both tokens are gone now.
  EXPECT_FALSE(ExistsToken(kExtensionAppId, kSender, kScope));
  EXPECT_FALSE(ExistsToken(kExtensionAppId, kSender, kScope));

  reset_last_event();

  // Trying to delete the token again will get an error.
  DeleteToken(kExtensionAppId, kSender, kScope);
  PumpLoopUntilIdle();

  EXPECT_EQ(UNREGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ(GCMClient::INVALID_PARAMETER, last_result());
}

TEST_F(GCMClientInstanceIDTest, DISABLED_DeleteAllTokens) {
  AddInstanceID(kExtensionAppId, kInstanceID);

  // Get a token.
  EXPECT_FALSE(ExistsToken(kExtensionAppId, kSender, kScope));
  GetToken(kExtensionAppId, kSender, kScope);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("token1"));

  EXPECT_EQ(REGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ("token1", last_registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  EXPECT_TRUE(ExistsToken(kExtensionAppId, kSender, kScope));

  reset_last_event();

  // Get another token.
  EXPECT_FALSE(ExistsToken(kExtensionAppId, kSender2, kScope));
  GetToken(kExtensionAppId, kSender2, kScope);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("token2"));

  EXPECT_EQ(REGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ("token2", last_registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  EXPECT_TRUE(ExistsToken(kExtensionAppId, kSender2, kScope));
  // The 1st token still exists.
  EXPECT_TRUE(ExistsToken(kExtensionAppId, kSender, kScope));

  reset_last_event();

  // Delete all tokens.
  DeleteToken(kExtensionAppId, "*", "*");
  ASSERT_NO_FATAL_FAILURE(CompleteDeleteToken());

  EXPECT_EQ(UNREGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  // All tokens are gone now.
  EXPECT_FALSE(ExistsToken(kExtensionAppId, kSender, kScope));
  EXPECT_FALSE(ExistsToken(kExtensionAppId, kSender, kScope));
}

TEST_F(GCMClientInstanceIDTest, DeleteAllTokensBeforeGetAnyToken) {
  AddInstanceID(kExtensionAppId, kInstanceID);

  // Delete all tokens without getting a token first.
  DeleteToken(kExtensionAppId, "*", "*");
  // No need to call CompleteDeleteToken since unregistration request should
  // not be triggered.
  PumpLoopUntilIdle();

  EXPECT_EQ(UNREGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
}

TEST_F(GCMClientInstanceIDTest, DispatchDownstreamMessageWithoutSubtype) {
  AddInstanceID(kExtensionAppId, kInstanceID);
  GetToken(kExtensionAppId, kSender, kScope);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("token1"));

  std::map<std::string, std::string> expected_data;

  MCSMessage message(BuildDownstreamMessage(
      kSender, kExtensionAppId, std::string() /* subtype */, expected_data,
      std::string() /* raw_data */));
  EXPECT_TRUE(message.IsValid());
  ReceiveMessageFromMCS(message);

  EXPECT_EQ(MESSAGE_RECEIVED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ(expected_data.size(), last_message().data.size());
  EXPECT_EQ(expected_data, last_message().data);
  EXPECT_EQ(kSender, last_message().sender_id);
}

TEST_F(GCMClientInstanceIDTest, DispatchDownstreamMessageWithSubtype) {
  AddInstanceID(kSubtypeAppId, kInstanceID);
  GetToken(kSubtypeAppId, kSender, kScope);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("token1"));

  std::map<std::string, std::string> expected_data;

  MCSMessage message(BuildDownstreamMessage(
      kSender, kProductCategoryForSubtypes, kSubtypeAppId /* subtype */,
      expected_data, std::string() /* raw_data */));
  EXPECT_TRUE(message.IsValid());
  ReceiveMessageFromMCS(message);

  EXPECT_EQ(MESSAGE_RECEIVED, last_event());
  EXPECT_EQ(kSubtypeAppId, last_app_id());
  EXPECT_EQ(expected_data.size(), last_message().data.size());
  EXPECT_EQ(expected_data, last_message().data);
  EXPECT_EQ(kSender, last_message().sender_id);
}

TEST_F(GCMClientInstanceIDTest, DispatchDownstreamMessageWithFakeSubtype) {
  // Victim non-extension registration.
  AddInstanceID(kSubtypeAppId, "iid_1");
  GetToken(kSubtypeAppId, kSender, kScope);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("token1"));

  // Malicious extension registration.
  AddInstanceID(kExtensionAppId, "iid_2");
  GetToken(kExtensionAppId, kSender, kScope);
  ASSERT_NO_FATAL_FAILURE(CompleteRegistration("token2"));

  std::map<std::string, std::string> expected_data;

  // Message for kExtensionAppId should be delivered to the extension rather
  // than the victim app, despite the malicious subtype property attempting to
  // impersonate victim app.
  MCSMessage message(BuildDownstreamMessage(
      kSender, kExtensionAppId /* category */, kSubtypeAppId /* subtype */,
      expected_data, std::string() /* raw_data */));
  EXPECT_TRUE(message.IsValid());
  ReceiveMessageFromMCS(message);

  EXPECT_EQ(MESSAGE_RECEIVED, last_event());
  EXPECT_EQ(kExtensionAppId, last_app_id());
  EXPECT_EQ(expected_data.size(), last_message().data.size());
  EXPECT_EQ(expected_data, last_message().data);
  EXPECT_EQ(kSender, last_message().sender_id);
}

}  // namespace gcm
