// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_driver_desktop.h"

#include <stdint.h>

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "components/gcm_driver/crypto/gcm_encryption_provider.h"
#include "components/gcm_driver/fake_gcm_app_handler.h"
#include "components/gcm_driver/fake_gcm_client.h"
#include "components/gcm_driver/fake_gcm_client_factory.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_connection_observer.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const char kTestAppID1[] = "TestApp1";
const char kTestAppID2[] = "TestApp2";
const char kUserID1[] = "user1";
const char kScope[] = "GCM";
const char kInstanceID1[] = "IID1";
const char kInstanceID2[] = "IID2";

class FakeGCMConnectionObserver : public GCMConnectionObserver {
 public:
  FakeGCMConnectionObserver();
  ~FakeGCMConnectionObserver() override;

  // gcm::GCMConnectionObserver implementation:
  void OnConnected(const net::IPEndPoint& ip_endpoint) override;
  void OnDisconnected() override;

  bool connected() const { return connected_; }

 private:
  bool connected_;
};

FakeGCMConnectionObserver::FakeGCMConnectionObserver() : connected_(false) {
}

FakeGCMConnectionObserver::~FakeGCMConnectionObserver() {
}

void FakeGCMConnectionObserver::OnConnected(
    const net::IPEndPoint& ip_endpoint) {
  connected_ = true;
}

void FakeGCMConnectionObserver::OnDisconnected() {
  connected_ = false;
}

void PumpCurrentLoop() {
  base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
}

void PumpUILoop() {
  PumpCurrentLoop();
}

std::vector<std::string> ToSenderList(const std::string& sender_ids) {
  return base::SplitString(
      sender_ids, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

}  // namespace

class GCMDriverTest : public testing::Test {
 public:
  enum WaitToFinish {
    DO_NOT_WAIT,
    WAIT
  };

  GCMDriverTest();

  GCMDriverTest(const GCMDriverTest&) = delete;
  GCMDriverTest& operator=(const GCMDriverTest&) = delete;

  ~GCMDriverTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  GCMDriverDesktop* driver() { return driver_.get(); }
  FakeGCMAppHandler* gcm_app_handler() { return gcm_app_handler_.get(); }
  FakeGCMConnectionObserver* gcm_connection_observer() {
    return gcm_connection_observer_.get();
  }
  const std::string& registration_id() const { return registration_id_; }
  GCMClient::Result registration_result() const { return registration_result_; }
  const std::string& send_message_id() const { return send_message_id_; }
  GCMClient::Result send_result() const { return send_result_; }
  GCMClient::Result unregistration_result() const {
    return unregistration_result_;
  }
  const std::string& p256dh() const { return p256dh_; }
  const std::string& auth_secret() const { return auth_secret_; }

  void PumpIOLoop();

  void ClearResults();

  bool HasAppHandlers() const;
  FakeGCMClient* GetGCMClient();

  void CreateDriver();
  void ShutdownDriver();
  void AddAppHandlers();
  void RemoveAppHandlers();

  void Register(const std::string& app_id,
                const std::vector<std::string>& sender_ids,
                WaitToFinish wait_to_finish);
  void Send(const std::string& app_id,
            const std::string& receiver_id,
            const OutgoingMessage& message,
            WaitToFinish wait_to_finish);
  void GetEncryptionInfo(const std::string& app_id,
                         WaitToFinish wait_to_finish);
  void Unregister(const std::string& app_id, WaitToFinish wait_to_finish);

  void WaitForAsyncOperation();

  void RegisterCompleted(const std::string& registration_id,
                         GCMClient::Result result);
  void SendCompleted(const std::string& message_id, GCMClient::Result result);
  void GetEncryptionInfoCompleted(std::string p256dh, std::string auth_secret);
  void UnregisterCompleted(GCMClient::Result result);

  void AsyncOperationCompleted() {
    if (async_operation_completed_callback_)
      std::move(async_operation_completed_callback_).Run();
  }
  void set_async_operation_completed_callback(base::OnceClosure callback) {
    async_operation_completed_callback_ = std::move(callback);
  }

 private:
  base::ScopedTempDir temp_dir_;
  TestingPrefServiceSimple prefs_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  base::Thread io_thread_;
  network::TestURLLoaderFactory test_url_loader_factory_;

  std::unique_ptr<GCMDriverDesktop> driver_;
  std::unique_ptr<FakeGCMAppHandler> gcm_app_handler_;
  std::unique_ptr<FakeGCMConnectionObserver> gcm_connection_observer_;

  base::OnceClosure async_operation_completed_callback_;

  std::string registration_id_;
  GCMClient::Result registration_result_;
  std::string send_message_id_;
  GCMClient::Result send_result_;
  GCMClient::Result unregistration_result_;
  std::string p256dh_;
  std::string auth_secret_;
};

GCMDriverTest::GCMDriverTest()
    : io_thread_("IOThread"),
      registration_result_(GCMClient::UNKNOWN_ERROR),
      send_result_(GCMClient::UNKNOWN_ERROR),
      unregistration_result_(GCMClient::UNKNOWN_ERROR) {}

GCMDriverTest::~GCMDriverTest() {
}

void GCMDriverTest::SetUp() {
  io_thread_.Start();
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
}

void GCMDriverTest::TearDown() {
  if (!driver_)
    return;

  ShutdownDriver();
  driver_.reset();
  PumpIOLoop();

  io_thread_.Stop();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(temp_dir_.Delete());
}

void GCMDriverTest::PumpIOLoop() {
  base::RunLoop run_loop;
  io_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&PumpCurrentLoop), run_loop.QuitClosure());
  run_loop.Run();
}

void GCMDriverTest::ClearResults() {
  registration_id_.clear();
  registration_result_ = GCMClient::UNKNOWN_ERROR;

  send_message_id_.clear();
  send_result_ = GCMClient::UNKNOWN_ERROR;

  unregistration_result_ = GCMClient::UNKNOWN_ERROR;
}

bool GCMDriverTest::HasAppHandlers() const {
  return !driver_->app_handlers().empty();
}

FakeGCMClient* GCMDriverTest::GetGCMClient() {
  return static_cast<FakeGCMClient*>(driver_->GetGCMClientForTesting());
}

void GCMDriverTest::CreateDriver() {
  GCMClient::ChromeBuildInfo chrome_build_info;
  chrome_build_info.product_category_for_subtypes = "com.chrome.macosx";
  driver_ = std::make_unique<GCMDriverDesktop>(
      std::unique_ptr<GCMClientFactory>(new FakeGCMClientFactory(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          io_thread_.task_runner())),
      chrome_build_info, &prefs_, temp_dir_.GetPath(), base::DoNothing(),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_),
      network::TestNetworkConnectionTracker::GetInstance(),
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      io_thread_.task_runner(), task_environment_.GetMainThreadTaskRunner());

  gcm_app_handler_ = std::make_unique<FakeGCMAppHandler>();
  gcm_connection_observer_ = std::make_unique<FakeGCMConnectionObserver>();

  driver_->AddConnectionObserver(gcm_connection_observer_.get());
}

void GCMDriverTest::ShutdownDriver() {
  if (gcm_connection_observer())
    driver()->RemoveConnectionObserver(gcm_connection_observer());
  driver()->Shutdown();
}

void GCMDriverTest::AddAppHandlers() {
  driver_->AddAppHandler(kTestAppID1, gcm_app_handler_.get());
  driver_->AddAppHandler(kTestAppID2, gcm_app_handler_.get());
}

void GCMDriverTest::RemoveAppHandlers() {
  driver_->RemoveAppHandler(kTestAppID1);
  driver_->RemoveAppHandler(kTestAppID2);
}

void GCMDriverTest::Register(const std::string& app_id,
                             const std::vector<std::string>& sender_ids,
                             WaitToFinish wait_to_finish) {
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  driver_->Register(app_id, sender_ids,
                    base::BindOnce(&GCMDriverTest::RegisterCompleted,
                                   base::Unretained(this)));
  if (wait_to_finish == WAIT)
    run_loop.Run();
}

void GCMDriverTest::Send(const std::string& app_id,
                         const std::string& receiver_id,
                         const OutgoingMessage& message,
                         WaitToFinish wait_to_finish) {
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  driver_->Send(
      app_id, receiver_id, message,
      base::BindOnce(&GCMDriverTest::SendCompleted, base::Unretained(this)));
  if (wait_to_finish == WAIT)
    run_loop.Run();
}

void GCMDriverTest::GetEncryptionInfo(const std::string& app_id,
                                      WaitToFinish wait_to_finish) {
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  driver_->GetEncryptionInfo(
      app_id, base::BindOnce(&GCMDriverTest::GetEncryptionInfoCompleted,
                             base::Unretained(this)));
  if (wait_to_finish == WAIT)
    run_loop.Run();
}

void GCMDriverTest::Unregister(const std::string& app_id,
                               WaitToFinish wait_to_finish) {
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  driver_->Unregister(app_id,
                      base::BindOnce(&GCMDriverTest::UnregisterCompleted,
                                     base::Unretained(this)));
  if (wait_to_finish == WAIT)
    run_loop.Run();
}

void GCMDriverTest::WaitForAsyncOperation() {
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

void GCMDriverTest::RegisterCompleted(const std::string& registration_id,
                                      GCMClient::Result result) {
  registration_id_ = registration_id;
  registration_result_ = result;
  AsyncOperationCompleted();
}

void GCMDriverTest::SendCompleted(const std::string& message_id,
                                  GCMClient::Result result) {
  send_message_id_ = message_id;
  send_result_ = result;
  AsyncOperationCompleted();
}

void GCMDriverTest::GetEncryptionInfoCompleted(std::string p256dh,
                                               std::string auth_secret) {
  p256dh_ = std::move(p256dh);
  auth_secret_ = std::move(auth_secret);
  AsyncOperationCompleted();
}

void GCMDriverTest::UnregisterCompleted(GCMClient::Result result) {
  unregistration_result_ = result;
  AsyncOperationCompleted();
}

TEST_F(GCMDriverTest, Create) {
  // Create GCMDriver first. By default GCM is set to delay start.
  CreateDriver();
  EXPECT_FALSE(driver()->IsStarted());

  // Adding an app handler will not start GCM.
  AddAppHandlers();
  PumpIOLoop();
  PumpUILoop();
  EXPECT_FALSE(driver()->IsStarted());
  EXPECT_FALSE(driver()->IsConnected());
  EXPECT_FALSE(gcm_connection_observer()->connected());

  // The GCM registration will kick off the GCM.
  Register(kTestAppID1, ToSenderList("sender"), GCMDriverTest::WAIT);
  EXPECT_TRUE(driver()->IsStarted());
  EXPECT_TRUE(driver()->IsConnected());
  EXPECT_TRUE(gcm_connection_observer()->connected());
}

TEST_F(GCMDriverTest, Shutdown) {
  CreateDriver();
  EXPECT_FALSE(HasAppHandlers());

  AddAppHandlers();
  EXPECT_TRUE(HasAppHandlers());

  ShutdownDriver();
  EXPECT_FALSE(HasAppHandlers());
  EXPECT_FALSE(driver()->IsConnected());
  EXPECT_FALSE(gcm_connection_observer()->connected());
}

TEST_F(GCMDriverTest, StartOrStopGCMOnDemand) {
  CreateDriver();
  PumpIOLoop();
  PumpUILoop();
  EXPECT_FALSE(driver()->IsStarted());

  // Adding an app handler will not start GCM.
  driver()->AddAppHandler(kTestAppID1, gcm_app_handler());
  PumpIOLoop();
  PumpUILoop();
  EXPECT_FALSE(driver()->IsStarted());

  // The GCM registration will kick off the GCM.
  Register(kTestAppID1, ToSenderList("sender"), GCMDriverTest::WAIT);
  EXPECT_TRUE(driver()->IsStarted());

  // Add another app handler.
  driver()->AddAppHandler(kTestAppID2, gcm_app_handler());
  PumpIOLoop();
  PumpUILoop();
  EXPECT_TRUE(driver()->IsStarted());

  // GCMClient remains active after one app handler is gone.
  driver()->RemoveAppHandler(kTestAppID1);
  PumpIOLoop();
  PumpUILoop();
  EXPECT_TRUE(driver()->IsStarted());

  // GCMClient should be stopped after the last app handler is gone.
  driver()->RemoveAppHandler(kTestAppID2);
  PumpIOLoop();
  PumpUILoop();
  EXPECT_FALSE(driver()->IsStarted());

  // GCMClient is restarted after an app handler has been added.
  driver()->AddAppHandler(kTestAppID2, gcm_app_handler());
  PumpIOLoop();
  PumpUILoop();
  EXPECT_TRUE(driver()->IsStarted());
}

TEST_F(GCMDriverTest, RegisterFailed) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");

  CreateDriver();

  // Registration fails when the no app handler is added.
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);
  EXPECT_TRUE(registration_id().empty());
  EXPECT_EQ(GCMClient::UNKNOWN_ERROR, registration_result());
}

TEST_F(GCMDriverTest, UnregisterFailed) {
  CreateDriver();

  // Unregistration fails when the no app handler is added.
  Unregister(kTestAppID1, GCMDriverTest::WAIT);
  EXPECT_EQ(GCMClient::UNKNOWN_ERROR, unregistration_result());
}

TEST_F(GCMDriverTest, SendFailed) {
  OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";

  CreateDriver();

  // Sending fails when the no app handler is added.
  Send(kTestAppID1, kUserID1, message, GCMDriverTest::WAIT);
  EXPECT_TRUE(send_message_id().empty());
  EXPECT_EQ(GCMClient::UNKNOWN_ERROR, send_result());
}

TEST_F(GCMDriverTest, GCMClientNotReadyBeforeRegistration) {
  CreateDriver();
  PumpIOLoop();
  PumpUILoop();

  // Make GCMClient not ready until PerformDelayedStart is called.
  GetGCMClient()->set_start_mode_overridding(
      FakeGCMClient::FORCE_TO_ALWAYS_DELAY_START_GCM);

  AddAppHandlers();

  // The registration is on hold until GCMClient is ready.
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(kTestAppID1,
                     sender_ids,
                     GCMDriverTest::DO_NOT_WAIT);
  PumpIOLoop();
  PumpUILoop();
  EXPECT_TRUE(registration_id().empty());
  EXPECT_EQ(GCMClient::UNKNOWN_ERROR, registration_result());

  // Register operation will be invoked after GCMClient becomes ready.
  GetGCMClient()->PerformDelayedStart();
  WaitForAsyncOperation();
  EXPECT_FALSE(registration_id().empty());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());
}

TEST_F(GCMDriverTest, GCMClientNotReadyBeforeSending) {
  CreateDriver();
  PumpIOLoop();
  PumpUILoop();

  // Make GCMClient not ready until PerformDelayedStart is called.
  GetGCMClient()->set_start_mode_overridding(
      FakeGCMClient::FORCE_TO_ALWAYS_DELAY_START_GCM);

  AddAppHandlers();

  // The sending is on hold until GCMClient is ready.
  OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  Send(kTestAppID1, kUserID1, message, GCMDriverTest::DO_NOT_WAIT);
  PumpIOLoop();
  PumpUILoop();

  EXPECT_TRUE(send_message_id().empty());
  EXPECT_EQ(GCMClient::UNKNOWN_ERROR, send_result());

  // Send operation will be invoked after GCMClient becomes ready.
  GetGCMClient()->PerformDelayedStart();
  WaitForAsyncOperation();
  EXPECT_EQ(message.id, send_message_id());
  EXPECT_EQ(GCMClient::SUCCESS, send_result());
}

// Tests a single instance of GCMDriver.
class GCMDriverFunctionalTest : public GCMDriverTest {
 public:
  GCMDriverFunctionalTest();

  GCMDriverFunctionalTest(const GCMDriverFunctionalTest&) = delete;
  GCMDriverFunctionalTest& operator=(const GCMDriverFunctionalTest&) = delete;

  ~GCMDriverFunctionalTest() override;

  // GCMDriverTest:
  void SetUp() override;
};

GCMDriverFunctionalTest::GCMDriverFunctionalTest() {
}

GCMDriverFunctionalTest::~GCMDriverFunctionalTest() {
}

void GCMDriverFunctionalTest::SetUp() {
  GCMDriverTest::SetUp();

  CreateDriver();
  AddAppHandlers();
  PumpIOLoop();
  PumpUILoop();
}

TEST_F(GCMDriverFunctionalTest, Register) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);
  const std::string expected_registration_id =
      FakeGCMClient::GenerateGCMRegistrationID(sender_ids);

  EXPECT_EQ(expected_registration_id, registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());
}

TEST_F(GCMDriverFunctionalTest, RegisterError) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1@error");
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);

  EXPECT_TRUE(registration_id().empty());
  EXPECT_NE(GCMClient::SUCCESS, registration_result());
}

TEST_F(GCMDriverFunctionalTest, RegisterAgainWithSameSenderIDs) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  sender_ids.push_back("sender2");
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);
  const std::string expected_registration_id =
      FakeGCMClient::GenerateGCMRegistrationID(sender_ids);

  EXPECT_EQ(expected_registration_id, registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());

  // Clears the results the would be set by the Register callback in preparation
  // to call register 2nd time.
  ClearResults();

  // Calling register 2nd time with the same set of sender IDs but different
  // ordering will get back the same registration ID.
  std::vector<std::string> another_sender_ids;
  another_sender_ids.push_back("sender2");
  another_sender_ids.push_back("sender1");
  Register(kTestAppID1, another_sender_ids, GCMDriverTest::WAIT);

  EXPECT_EQ(expected_registration_id, registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());
}

TEST_F(GCMDriverFunctionalTest, RegisterAgainWithDifferentSenderIDs) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);
  const std::string expected_registration_id =
      FakeGCMClient::GenerateGCMRegistrationID(sender_ids);

  EXPECT_EQ(expected_registration_id, registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());

  // Make sender IDs different.
  sender_ids.push_back("sender2");
  const std::string expected_registration_id2 =
      FakeGCMClient::GenerateGCMRegistrationID(sender_ids);

  // Calling register 2nd time with the different sender IDs will get back a new
  // registration ID.
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);
  EXPECT_EQ(expected_registration_id2, registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());
}

TEST_F(GCMDriverFunctionalTest, UnregisterExplicitly) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);

  EXPECT_FALSE(registration_id().empty());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());

  Unregister(kTestAppID1, GCMDriverTest::WAIT);

  EXPECT_EQ(GCMClient::SUCCESS, unregistration_result());
}

// TODO(crbug.com/40650420): Test is failing on ASan build.
#if defined(ADDRESS_SANITIZER)
TEST_F(GCMDriverFunctionalTest, DISABLED_UnregisterRemovesEncryptionInfo) {
#else
TEST_F(GCMDriverFunctionalTest, UnregisterRemovesEncryptionInfo) {
#endif
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);

  EXPECT_FALSE(registration_id().empty());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());

  GetEncryptionInfo(kTestAppID1, GCMDriverTest::WAIT);

  EXPECT_FALSE(p256dh().empty());
  EXPECT_FALSE(auth_secret().empty());

  const std::string app_p256dh = p256dh();
  const std::string app_auth_secret = auth_secret();

  GetEncryptionInfo(kTestAppID1, GCMDriverTest::WAIT);

  EXPECT_EQ(app_p256dh, p256dh());
  EXPECT_EQ(app_auth_secret, auth_secret());

  Unregister(kTestAppID1, GCMDriverTest::WAIT);

  EXPECT_EQ(GCMClient::SUCCESS, unregistration_result());

  GetEncryptionInfo(kTestAppID1, GCMDriverTest::WAIT);

  // The GCMKeyStore eagerly creates new keying material for registrations that
  // don't have any associated with them, so the most appropriate check to do is
  // to verify that the returned material is different from before.

  EXPECT_NE(app_p256dh, p256dh());
  EXPECT_NE(app_auth_secret, auth_secret());
}

TEST_F(GCMDriverFunctionalTest, UnregisterWhenAsyncOperationPending) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  // First start registration without waiting for it to complete.
  Register(kTestAppID1, sender_ids, GCMDriverTest::DO_NOT_WAIT);

  // Test that unregistration fails with async operation pending when there is a
  // registration already in progress.
  Unregister(kTestAppID1, GCMDriverTest::WAIT);
  EXPECT_EQ(GCMClient::ASYNC_OPERATION_PENDING,
            unregistration_result());

  // Complete the unregistration.
  WaitForAsyncOperation();
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());

  // Start unregistration without waiting for it to complete. This time no async
  // operation is pending.
  Unregister(kTestAppID1, GCMDriverTest::DO_NOT_WAIT);

  // Test that unregistration fails with async operation pending when there is
  // an unregistration already in progress.
  Unregister(kTestAppID1, GCMDriverTest::WAIT);
  EXPECT_EQ(GCMClient::ASYNC_OPERATION_PENDING,
            unregistration_result());
  ClearResults();

  // Complete unregistration.
  WaitForAsyncOperation();
  EXPECT_EQ(GCMClient::SUCCESS, unregistration_result());
}

TEST_F(GCMDriverFunctionalTest, RegisterWhenAsyncOperationPending) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  // First start registration without waiting for it to complete.
  Register(kTestAppID1, sender_ids, GCMDriverTest::DO_NOT_WAIT);

  // Test that registration fails with async operation pending when there is a
  // registration already in progress.
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);
  EXPECT_EQ(GCMClient::ASYNC_OPERATION_PENDING,
            registration_result());
  ClearResults();

  // Complete the registration.
  WaitForAsyncOperation();
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());
}

TEST_F(GCMDriverFunctionalTest, RegisterAfterUnfinishedUnregister) {
  // Register and wait for it to complete.
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());
  EXPECT_EQ(FakeGCMClient::GenerateGCMRegistrationID(sender_ids),
            registration_id());

  // Clears the results the would be set by the Register callback in preparation
  // to call register 2nd time.
  ClearResults();

  // Start unregistration without waiting for it to complete.
  Unregister(kTestAppID1, GCMDriverTest::DO_NOT_WAIT);

  // Register immediately after unregistration is not completed.
  sender_ids.push_back("sender2");
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);

  // We need one more waiting since the waiting in Register is indeed for
  // uncompleted Unregister.
  WaitForAsyncOperation();
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());
  EXPECT_EQ(FakeGCMClient::GenerateGCMRegistrationID(sender_ids),
            registration_id());
}

TEST_F(GCMDriverFunctionalTest, Send) {
  OutgoingMessage message;
  message.id = "1@ack";
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  Send(kTestAppID1, kUserID1, message, GCMDriverTest::WAIT);

  EXPECT_EQ(message.id, send_message_id());
  EXPECT_EQ(GCMClient::SUCCESS, send_result());

  gcm_app_handler()->WaitForNotification();
  EXPECT_EQ(message.id, gcm_app_handler()->acked_message_id());
  EXPECT_EQ(kTestAppID1, gcm_app_handler()->app_id());
}

TEST_F(GCMDriverFunctionalTest, SendError) {
  OutgoingMessage message;
  // Embedding error in id will tell the mock to simulate the send error.
  message.id = "1@error";
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  Send(kTestAppID1, kUserID1, message, GCMDriverTest::WAIT);

  EXPECT_EQ(message.id, send_message_id());
  EXPECT_EQ(GCMClient::SUCCESS, send_result());

  // Wait for the send error.
  gcm_app_handler()->WaitForNotification();
  EXPECT_EQ(FakeGCMAppHandler::SEND_ERROR_EVENT,
            gcm_app_handler()->received_event());
  EXPECT_EQ(kTestAppID1, gcm_app_handler()->app_id());
  EXPECT_EQ(message.id,
            gcm_app_handler()->send_error_details().message_id);
  EXPECT_NE(GCMClient::SUCCESS,
            gcm_app_handler()->send_error_details().result);
  EXPECT_EQ(message.data,
            gcm_app_handler()->send_error_details().additional_data);
}

TEST_F(GCMDriverFunctionalTest, MessageReceived) {
  // GCM registration has to be performed otherwise GCM will not be started.
  Register(kTestAppID1, ToSenderList("sender"), GCMDriverTest::WAIT);

  IncomingMessage message;
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  message.sender_id = "sender";
  GetGCMClient()->ReceiveMessage(kTestAppID1, message);
  gcm_app_handler()->WaitForNotification();
  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            gcm_app_handler()->received_event());
  EXPECT_EQ(kTestAppID1, gcm_app_handler()->app_id());
  EXPECT_EQ(message.data, gcm_app_handler()->message().data);
  EXPECT_TRUE(gcm_app_handler()->message().collapse_key.empty());
  EXPECT_EQ(message.sender_id, gcm_app_handler()->message().sender_id);
}

TEST_F(GCMDriverFunctionalTest, MessageWithCollapseKeyReceived) {
  // GCM registration has to be performed otherwise GCM will not be started.
  Register(kTestAppID1, ToSenderList("sender"), GCMDriverTest::WAIT);

  IncomingMessage message;
  message.data["key1"] = "value1";
  message.collapse_key = "collapse_key_value";
  message.sender_id = "sender";
  GetGCMClient()->ReceiveMessage(kTestAppID1, message);
  gcm_app_handler()->WaitForNotification();
  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            gcm_app_handler()->received_event());
  EXPECT_EQ(kTestAppID1, gcm_app_handler()->app_id());
  EXPECT_EQ(message.data, gcm_app_handler()->message().data);
  EXPECT_EQ(message.collapse_key,
            gcm_app_handler()->message().collapse_key);
}

TEST_F(GCMDriverFunctionalTest, EncryptedMessageReceivedError) {
  // GCM registration has to be performed otherwise GCM will not be started.
  Register(kTestAppID1, ToSenderList("sender"), GCMDriverTest::WAIT);

  IncomingMessage message;

  // All required information to trigger the encryption path, but with an
  // invalid Crypto-Key header value to trigger an error.
  message.data["encryption"] = "salt=ysyxqlYTgE0WvcZrmHbUbg";
  message.data["crypto-key"] = "hey=thereisnopublickey";
  message.sender_id = "sender";
  message.raw_data = "foobar";

  GetGCMClient()->SetRecording(true);
  GetGCMClient()->ReceiveMessage(kTestAppID1, message);

  PumpIOLoop();
  PumpUILoop();
  PumpIOLoop();

  EXPECT_EQ(FakeGCMAppHandler::DECRYPTION_FAILED_EVENT,
            gcm_app_handler()->received_event());

  GCMClient::GCMStatistics statistics = GetGCMClient()->GetStatistics();
  EXPECT_TRUE(statistics.is_recording);
  EXPECT_EQ(
      1u, statistics.recorded_activities.decryption_failure_activities.size());
}

TEST_F(GCMDriverFunctionalTest, MessagesDeleted) {
  // GCM registration has to be performed otherwise GCM will not be started.
  Register(kTestAppID1, ToSenderList("sender"), GCMDriverTest::WAIT);

  GetGCMClient()->DeleteMessages(kTestAppID1);
  gcm_app_handler()->WaitForNotification();
  EXPECT_EQ(FakeGCMAppHandler::MESSAGES_DELETED_EVENT,
            gcm_app_handler()->received_event());
  EXPECT_EQ(kTestAppID1, gcm_app_handler()->app_id());
}

TEST_F(GCMDriverFunctionalTest, LastTokenFetchTime) {
  // GCM registration has to be performed otherwise GCM will not be started.
  Register(kTestAppID1, ToSenderList("sender"), GCMDriverTest::WAIT);

  EXPECT_EQ(base::Time(), driver()->GetLastTokenFetchTime());
  base::Time fetch_time = base::Time::Now();
  driver()->SetLastTokenFetchTime(fetch_time);
  EXPECT_EQ(fetch_time, driver()->GetLastTokenFetchTime());
}

class GCMDriverInstanceIDTest : public GCMDriverTest {
 public:
  GCMDriverInstanceIDTest();

  GCMDriverInstanceIDTest(const GCMDriverInstanceIDTest&) = delete;
  GCMDriverInstanceIDTest& operator=(const GCMDriverInstanceIDTest&) = delete;

  ~GCMDriverInstanceIDTest() override;

  void GetReady();
  void GetInstanceID(const std::string& app_id, WaitToFinish wait_to_finish);
  void GetInstanceIDDataCompleted(const std::string& instance_id,
                                  const std::string& extra_data);
  void GetToken(const std::string& app_id,
                const std::string& authorized_entity,
                const std::string& scope,
                WaitToFinish wait_to_finish);
  void DeleteToken(const std::string& app_id,
                   const std::string& authorized_entity,
                   const std::string& scope,
                   WaitToFinish wait_to_finish);
  void AddInstanceIDData(const std::string& app_id,
                         const std::string& instance_id,
                         const std::string& extra_data);
  void RemoveInstanceIDData(const std::string& app_id);

  std::string instance_id() const { return instance_id_; }
  std::string extra_data() const { return extra_data_; }

  int instance_id_resolved_counter() const {
    return instance_id_resolved_counter_;
  }

 private:
  std::string instance_id_;
  std::string extra_data_;

  int instance_id_resolved_counter_ = 0;
};

GCMDriverInstanceIDTest::GCMDriverInstanceIDTest() {
}

GCMDriverInstanceIDTest::~GCMDriverInstanceIDTest() {
}

void GCMDriverInstanceIDTest::GetReady() {
  CreateDriver();
  AddAppHandlers();
  PumpIOLoop();
  PumpUILoop();
}

void GCMDriverInstanceIDTest::GetInstanceID(const std::string& app_id,
                                            WaitToFinish wait_to_finish) {
  base::RunLoop run_loop;
  set_async_operation_completed_callback(run_loop.QuitClosure());
  driver()->GetInstanceIDHandlerInternal()->GetInstanceIDData(
      app_id,
      base::BindOnce(&GCMDriverInstanceIDTest::GetInstanceIDDataCompleted,
                     base::Unretained(this)));
  if (wait_to_finish == WAIT)
    run_loop.Run();
}

void GCMDriverInstanceIDTest::GetInstanceIDDataCompleted(
    const std::string& instance_id, const std::string& extra_data) {
  instance_id_ = instance_id;
  extra_data_ = extra_data;

  instance_id_resolved_counter_++;

  AsyncOperationCompleted();
}

void GCMDriverInstanceIDTest::GetToken(const std::string& app_id,
                                       const std::string& authorized_entity,
                                       const std::string& scope,
                                       WaitToFinish wait_to_finish) {
  base::RunLoop run_loop;
  set_async_operation_completed_callback(run_loop.QuitClosure());
  driver()->GetInstanceIDHandlerInternal()->GetToken(
      app_id, authorized_entity, scope, /*time_to_live=*/base::TimeDelta(),
      base::BindOnce(&GCMDriverTest::RegisterCompleted,
                     base::Unretained(this)));
  if (wait_to_finish == WAIT)
    run_loop.Run();
}

void GCMDriverInstanceIDTest::DeleteToken(const std::string& app_id,
                                          const std::string& authorized_entity,
                                          const std::string& scope,
                                          WaitToFinish wait_to_finish) {
  base::RunLoop run_loop;
  set_async_operation_completed_callback(run_loop.QuitClosure());
  driver()->GetInstanceIDHandlerInternal()->DeleteToken(
      app_id, authorized_entity, scope,
      base::BindOnce(&GCMDriverTest::UnregisterCompleted,
                     base::Unretained(this)));
  if (wait_to_finish == WAIT)
    run_loop.Run();
}

void GCMDriverInstanceIDTest::AddInstanceIDData(const std::string& app_id,
                                                const std::string& instance_id,
                                                const std::string& extra_data) {
  driver()->GetInstanceIDHandlerInternal()->AddInstanceIDData(
      app_id, instance_id, extra_data);
}

void GCMDriverInstanceIDTest::RemoveInstanceIDData(const std::string& app_id) {
  driver()->GetInstanceIDHandlerInternal()->RemoveInstanceIDData(app_id);
}

TEST_F(GCMDriverInstanceIDTest, InstanceIDData) {
  GetReady();

  AddInstanceIDData(kTestAppID1, kInstanceID1, "Foo");
  GetInstanceID(kTestAppID1, GCMDriverTest::WAIT);

  EXPECT_EQ(kInstanceID1, instance_id());
  EXPECT_EQ("Foo", extra_data());
  EXPECT_EQ(1, instance_id_resolved_counter());

  RemoveInstanceIDData(kTestAppID1);
  GetInstanceID(kTestAppID1, GCMDriverTest::WAIT);

  EXPECT_TRUE(instance_id().empty());
  EXPECT_TRUE(extra_data().empty());
  EXPECT_EQ(2, instance_id_resolved_counter());

  AddInstanceIDData(kTestAppID1, kInstanceID1, "Bar");
  GetInstanceID(kTestAppID1, GCMDriverTest::DO_NOT_WAIT);
  GetInstanceID(kTestAppID1, GCMDriverTest::DO_NOT_WAIT);

  WaitForAsyncOperation();
  WaitForAsyncOperation();

  EXPECT_EQ(kInstanceID1, instance_id());
  EXPECT_EQ("Bar", extra_data());
  EXPECT_EQ(4, instance_id_resolved_counter());
}

TEST_F(GCMDriverInstanceIDTest, GCMClientNotReadyBeforeInstanceIDData) {
  CreateDriver();
  PumpIOLoop();
  PumpUILoop();

  // Make GCMClient not ready until PerformDelayedStart is called.
  GetGCMClient()->set_start_mode_overridding(
      FakeGCMClient::FORCE_TO_ALWAYS_DELAY_START_GCM);

  AddAppHandlers();

  // All operations are on hold until GCMClient is ready.
  AddInstanceIDData(kTestAppID1, kInstanceID1, "Foo");
  AddInstanceIDData(kTestAppID2, kInstanceID2, "Bar");
  RemoveInstanceIDData(kTestAppID1);
  GetInstanceID(kTestAppID2, GCMDriverTest::DO_NOT_WAIT);
  PumpIOLoop();
  PumpUILoop();
  EXPECT_TRUE(instance_id().empty());
  EXPECT_TRUE(extra_data().empty());

  // All operations will be performed after GCMClient becomes ready.
  GetGCMClient()->PerformDelayedStart();
  WaitForAsyncOperation();
  EXPECT_EQ(kInstanceID2, instance_id());
  EXPECT_EQ("Bar", extra_data());
}

TEST_F(GCMDriverInstanceIDTest, GetToken) {
  GetReady();

  const std::string expected_token =
      FakeGCMClient::GenerateInstanceIDToken(kUserID1, kScope);
  GetToken(kTestAppID1, kUserID1, kScope, GCMDriverTest::WAIT);

  EXPECT_EQ(expected_token, registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());
}

TEST_F(GCMDriverInstanceIDTest, GetTokenError) {
  GetReady();

  std::string error_entity = "sender@error";
  GetToken(kTestAppID1, error_entity, kScope, GCMDriverTest::WAIT);

  EXPECT_TRUE(registration_id().empty());
  EXPECT_NE(GCMClient::SUCCESS, registration_result());
}

TEST_F(GCMDriverInstanceIDTest, GCMClientNotReadyBeforeGetToken) {
  CreateDriver();
  PumpIOLoop();
  PumpUILoop();

  // Make GCMClient not ready until PerformDelayedStart is called.
  GetGCMClient()->set_start_mode_overridding(
      FakeGCMClient::FORCE_TO_ALWAYS_DELAY_START_GCM);

  AddAppHandlers();

  // GetToken operation is on hold until GCMClient is ready.
  GetToken(kTestAppID1, kUserID1, kScope, GCMDriverTest::DO_NOT_WAIT);
  PumpIOLoop();
  PumpUILoop();
  EXPECT_TRUE(registration_id().empty());
  EXPECT_EQ(GCMClient::UNKNOWN_ERROR, registration_result());

  // GetToken operation will be invoked after GCMClient becomes ready.
  GetGCMClient()->PerformDelayedStart();
  WaitForAsyncOperation();
  EXPECT_FALSE(registration_id().empty());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());
}

TEST_F(GCMDriverInstanceIDTest, DeleteToken) {
  GetReady();

  const std::string expected_token =
      FakeGCMClient::GenerateInstanceIDToken(kUserID1, kScope);
  GetToken(kTestAppID1, kUserID1, kScope, GCMDriverTest::WAIT);
  EXPECT_EQ(expected_token, registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());

  DeleteToken(kTestAppID1, kUserID1, kScope, GCMDriverTest::WAIT);
  EXPECT_EQ(GCMClient::SUCCESS, unregistration_result());
}

TEST_F(GCMDriverInstanceIDTest, GCMClientNotReadyBeforeDeleteToken) {
  CreateDriver();
  PumpIOLoop();
  PumpUILoop();

  // Make GCMClient not ready until PerformDelayedStart is called.
  GetGCMClient()->set_start_mode_overridding(
      FakeGCMClient::FORCE_TO_ALWAYS_DELAY_START_GCM);

  AddAppHandlers();

  // DeleteToken operation is on hold until GCMClient is ready.
  DeleteToken(kTestAppID1, kUserID1, kScope, GCMDriverTest::DO_NOT_WAIT);
  PumpIOLoop();
  PumpUILoop();
  EXPECT_EQ(GCMClient::UNKNOWN_ERROR, unregistration_result());

  // DeleteToken operation will be invoked after GCMClient becomes ready.
  GetGCMClient()->PerformDelayedStart();
  WaitForAsyncOperation();
  EXPECT_EQ(GCMClient::SUCCESS, unregistration_result());
}

}  // namespace gcm
