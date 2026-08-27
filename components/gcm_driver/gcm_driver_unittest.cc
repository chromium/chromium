// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/base64.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "components/gcm_driver/crypto/gcm_decryption_result.h"
#include "components/gcm_driver/crypto/gcm_encryption_provider.h"
#include "components/gcm_driver/crypto/gcm_encryption_result.h"
#include "components/gcm_driver/fake_gcm_app_handler.h"
#include "components/gcm_driver/fake_gcm_client_factory.h"
#include "components/gcm_driver/features.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_driver_desktop.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "crypto/keypair.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

class PrefixTestGCMAppHandler : public FakeGCMAppHandler {
 public:
  explicit PrefixTestGCMAppHandler(const std::string& prefix)
      : prefix_(prefix) {}
  ~PrefixTestGCMAppHandler() override = default;

  bool CanHandle(const std::string& app_id) const override {
    return base::StartsWith(app_id, prefix_,
                            base::CompareCase::INSENSITIVE_ASCII);
  }

 private:
  std::string prefix_;
};

const char kTestAppID1[] = "TestApp1";
const char kTestAppID2[] = "TestApp2";

void PumpCurrentLoop() {
  base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
}

}  // namespace

class GCMDriverBaseTest : public testing::Test {
 public:
  enum WaitToFinish { DO_NOT_WAIT, WAIT };

  GCMDriverBaseTest();

  GCMDriverBaseTest(const GCMDriverBaseTest&) = delete;
  GCMDriverBaseTest& operator=(const GCMDriverBaseTest&) = delete;

  ~GCMDriverBaseTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  GCMDriverDesktop* driver() { return driver_.get(); }

  const std::string& p256dh() const { return p256dh_; }
  const std::string& auth_secret() const { return auth_secret_; }
  network::TestURLLoaderFactory& loader() { return test_url_loader_factory_; }
  GCMEncryptionResult encryption_result() { return encryption_result_; }
  const std::string& encrypted_message() { return encrypted_message_; }
  GCMDecryptionResult decryption_result() { return decryption_result_; }
  const std::string& decrypted_message() { return decrypted_message_; }

  void PumpIOLoop();

  void CreateDriver();
  void ShutdownDriver();

  void GetEncryptionInfo(const std::string& app_id,
                         WaitToFinish wait_to_finish);
  void EncryptMessage(const std::string& app_id,
                      const std::string& authorized_entity,
                      const std::string& p256dh,
                      const std::string& auth_secret,
                      const std::string& message,
                      WaitToFinish wait_to_finish);
  void DecryptMessage(const std::string& app_id,
                      const std::string& authorized_entity,
                      const std::string& message,
                      WaitToFinish wait_to_finish);

  void GetEncryptionInfoCompleted(std::string p256dh, std::string auth_secret);
  void EncryptMessageCompleted(GCMEncryptionResult result, std::string message);
  void DecryptMessageCompleted(GCMDecryptionResult result, std::string message);
  void UnregisterCompleted(GCMClient::Result result);

 protected:
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
  base::ScopedTempDir temp_dir_;
  TestingPrefServiceSimple prefs_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::Thread io_thread_;
  network::TestURLLoaderFactory test_url_loader_factory_;

  std::unique_ptr<GCMDriverDesktop> driver_;

  base::OnceClosure async_operation_completed_callback_;
  std::string p256dh_;
  std::string auth_secret_;

  GCMEncryptionResult encryption_result_ =
      GCMEncryptionResult::ENCRYPTION_FAILED;
  std::string encrypted_message_;
  GCMDecryptionResult decryption_result_ = GCMDecryptionResult::UNENCRYPTED;
  std::string decrypted_message_;
};

GCMDriverBaseTest::GCMDriverBaseTest()
    : os_crypt_(os_crypt_async::GetTestOSCryptAsyncForTesting(
          /*is_sync_for_unittests=*/true)),
      io_thread_("IOThread") {}

GCMDriverBaseTest::~GCMDriverBaseTest() = default;

void GCMDriverBaseTest::SetUp() {
  io_thread_.Start();
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  CreateDriver();
  PumpIOLoop();
  PumpCurrentLoop();
}

void GCMDriverBaseTest::TearDown() {
  if (!driver_)
    return;

  ShutdownDriver();
  driver_.reset();
  PumpIOLoop();

  io_thread_.Stop();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(temp_dir_.Delete());
}

void GCMDriverBaseTest::PumpIOLoop() {
  base::RunLoop run_loop;
  io_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&PumpCurrentLoop), run_loop.QuitClosure());
  run_loop.Run();
}

void GCMDriverBaseTest::CreateDriver() {
  GCMClient::ChromeBuildInfo chrome_build_info;
  chrome_build_info.product_category_for_subtypes = "com.chrome.macosx";
  driver_ = std::make_unique<GCMDriverDesktop>(
      std::make_unique<FakeGCMClientFactory>(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          io_thread_.task_runner()),
      chrome_build_info, &prefs_, temp_dir_.GetPath(), base::DoNothing(),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_),
      network::TestNetworkConnectionTracker::GetInstance(),
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      io_thread_.task_runner(), task_environment_.GetMainThreadTaskRunner(),
      os_crypt_.get());
}

void GCMDriverBaseTest::ShutdownDriver() {
  driver()->Shutdown();
}

void GCMDriverBaseTest::GetEncryptionInfo(const std::string& app_id,
                                          WaitToFinish wait_to_finish) {
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  driver_->GetEncryptionInfo(
      app_id, base::BindOnce(&GCMDriverBaseTest::GetEncryptionInfoCompleted,
                             base::Unretained(this)));
  if (wait_to_finish == WAIT)
    run_loop.Run();
}

void GCMDriverBaseTest::EncryptMessage(const std::string& app_id,
                                       const std::string& authorized_entity,
                                       const std::string& p256dh,
                                       const std::string& auth_secret,
                                       const std::string& message,
                                       WaitToFinish wait_to_finish) {
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();

  driver()->EncryptMessage(
      app_id, authorized_entity, p256dh, auth_secret, message,
      base::BindOnce(&GCMDriverBaseTest::EncryptMessageCompleted,
                     base::Unretained(this)));

  if (wait_to_finish == WAIT)
    run_loop.Run();
}

void GCMDriverBaseTest::DecryptMessage(const std::string& app_id,
                                       const std::string& authorized_entity,
                                       const std::string& message,
                                       WaitToFinish wait_to_finish) {
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();

  driver()->DecryptMessage(
      app_id, authorized_entity, message,
      base::BindOnce(&GCMDriverBaseTest::DecryptMessageCompleted,
                     base::Unretained(this)));

  if (wait_to_finish == WAIT)
    run_loop.Run();
}

void GCMDriverBaseTest::GetEncryptionInfoCompleted(std::string p256dh,
                                                   std::string auth_secret) {
  p256dh_ = std::move(p256dh);
  auth_secret_ = std::move(auth_secret);
  if (!async_operation_completed_callback_.is_null())
    std::move(async_operation_completed_callback_).Run();
}

void GCMDriverBaseTest::EncryptMessageCompleted(GCMEncryptionResult result,
                                                std::string message) {
  encryption_result_ = result;
  encrypted_message_ = std::move(message);
  if (!async_operation_completed_callback_.is_null())
    std::move(async_operation_completed_callback_).Run();
}

void GCMDriverBaseTest::DecryptMessageCompleted(GCMDecryptionResult result,
                                                std::string message) {
  decryption_result_ = result;
  decrypted_message_ = std::move(message);
  if (!async_operation_completed_callback_.is_null())
    std::move(async_operation_completed_callback_).Run();
}

TEST_F(GCMDriverBaseTest, EncryptionDecryptionRoundTrip) {
  GetEncryptionInfo(kTestAppID1, GCMDriverBaseTest::WAIT);

  std::string message = "payload";
  ASSERT_NO_FATAL_FAILURE(
      EncryptMessage(kTestAppID1, /* authorized_entity= */ "", p256dh(),
                     auth_secret(), message, GCMDriverBaseTest::WAIT));

  EXPECT_EQ(GCMEncryptionResult::ENCRYPTED_DRAFT_08, encryption_result());

  ASSERT_NO_FATAL_FAILURE(
      DecryptMessage(kTestAppID1, /* authorized_entity= */ "",
                     encrypted_message(), GCMDriverBaseTest::WAIT));

  EXPECT_EQ(GCMDecryptionResult::DECRYPTED_DRAFT_08, decryption_result());
  EXPECT_EQ(message, decrypted_message());
}

TEST_F(GCMDriverBaseTest, EncryptionError) {
  // Intentionally not creating encryption info.

  std::string message = "payload";
  ASSERT_NO_FATAL_FAILURE(
      EncryptMessage(kTestAppID1, /* authorized_entity= */ "", p256dh(),
                     auth_secret(), message, GCMDriverBaseTest::WAIT));

  EXPECT_EQ(GCMEncryptionResult::NO_KEYS, encryption_result());
}

TEST_F(GCMDriverBaseTest, BufferedMessages_FeatureDisabled_MessagesDropped) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kGCMMessageBuffering);

  IncomingMessage incoming_msg;
  incoming_msg.sender_id = "sender_123";
  incoming_msg.message_id = "msg_456";

  driver()->DispatchMessageForTesting(kTestAppID1, incoming_msg);
  EXPECT_EQ(0u, driver()->GetBufferedMessagesCountForTesting(kTestAppID1));

  FakeGCMAppHandler handler;
  driver()->AddAppHandler(kTestAppID1, &handler);
  base::ScopedClosureRunner unregister_handler(base::BindOnce(
      &GCMDriver::RemoveAppHandler, base::Unretained(driver()), kTestAppID1));

  EXPECT_TRUE(handler.received_messages().empty());
}

TEST_F(GCMDriverBaseTest, BufferedMessagesDispatchedOnAddAppHandler) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kGCMMessageBuffering);

  IncomingMessage incoming_msg;
  incoming_msg.sender_id = "sender_123";
  incoming_msg.message_id = "msg_456";

  // Dispatch message when no AppHandler is registered.
  driver()->DispatchMessageForTesting(kTestAppID1, incoming_msg);

  // Verify message is buffered.
  EXPECT_EQ(1u, driver()->GetBufferedMessagesCountForTesting(kTestAppID1));

  // Add AppHandler for kTestAppID1.
  FakeGCMAppHandler handler;
  driver()->AddAppHandler(kTestAppID1, &handler);
  base::ScopedClosureRunner unregister_handler(base::BindOnce(
      &GCMDriver::RemoveAppHandler, base::Unretained(driver()), kTestAppID1));

  // Verify message was delivered to handler and buffer cleared.
  EXPECT_EQ(0u, driver()->GetBufferedMessagesCountForTesting(kTestAppID1));
  ASSERT_EQ(1u, handler.received_messages().size());
  EXPECT_EQ("sender_123", handler.received_messages()[0].message.sender_id);
  EXPECT_EQ("msg_456", handler.received_messages()[0].message.message_id);
}

TEST_F(GCMDriverBaseTest,
       BufferedMessages_MultipleMessages_DeliveredInFIFOOrder) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kGCMMessageBuffering);

  constexpr size_t kNumMessages = 3;
  for (size_t i = 0; i < kNumMessages; ++i) {
    IncomingMessage msg;
    msg.sender_id = "sender_123";
    msg.message_id = base::StringPrintf("msg_%zu", i);
    driver()->DispatchMessageForTesting(kTestAppID1, msg);
  }

  EXPECT_EQ(kNumMessages,
            driver()->GetBufferedMessagesCountForTesting(kTestAppID1));

  FakeGCMAppHandler handler;
  driver()->AddAppHandler(kTestAppID1, &handler);
  base::ScopedClosureRunner unregister_handler(base::BindOnce(
      &GCMDriver::RemoveAppHandler, base::Unretained(driver()), kTestAppID1));

  EXPECT_EQ(0u, driver()->GetBufferedMessagesCountForTesting(kTestAppID1));
  ASSERT_EQ(kNumMessages, handler.received_messages().size());
  for (size_t i = 0; i < kNumMessages; ++i) {
    EXPECT_EQ("sender_123", handler.received_messages()[i].message.sender_id);
    EXPECT_EQ(base::StringPrintf("msg_%zu", i),
              handler.received_messages()[i].message.message_id);
  }
}

TEST_F(GCMDriverBaseTest, BufferedMessages_MultipleApps_IsolatedBuffers) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kGCMMessageBuffering);

  IncomingMessage msg1;
  msg1.message_id = "msg_app1";
  driver()->DispatchMessageForTesting(kTestAppID1, msg1);

  IncomingMessage msg2;
  msg2.message_id = "msg_app2";
  driver()->DispatchMessageForTesting(kTestAppID2, msg2);

  EXPECT_EQ(1u, driver()->GetBufferedMessagesCountForTesting(kTestAppID1));
  EXPECT_EQ(1u, driver()->GetBufferedMessagesCountForTesting(kTestAppID2));

  FakeGCMAppHandler handler1;
  driver()->AddAppHandler(kTestAppID1, &handler1);
  base::ScopedClosureRunner unregister1(base::BindOnce(
      &GCMDriver::RemoveAppHandler, base::Unretained(driver()), kTestAppID1));

  EXPECT_EQ(0u, driver()->GetBufferedMessagesCountForTesting(kTestAppID1));
  EXPECT_EQ(1u, driver()->GetBufferedMessagesCountForTesting(kTestAppID2));
  ASSERT_EQ(1u, handler1.received_messages().size());
  EXPECT_EQ("msg_app1", handler1.received_messages()[0].message.message_id);

  FakeGCMAppHandler handler2;
  driver()->AddAppHandler(kTestAppID2, &handler2);
  base::ScopedClosureRunner unregister2(base::BindOnce(
      &GCMDriver::RemoveAppHandler, base::Unretained(driver()), kTestAppID2));

  EXPECT_EQ(0u, driver()->GetBufferedMessagesCountForTesting(kTestAppID2));
  ASSERT_EQ(1u, handler2.received_messages().size());
  EXPECT_EQ("msg_app2", handler2.received_messages()[0].message.message_id);
}

TEST_F(GCMDriverBaseTest, BufferedMessages_PartialTTLExpiration) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kGCMMessageBuffering,
      {{"message_buffering_ttl_seconds", "120"}});

  IncomingMessage msg1;
  msg1.message_id = "msg_expired";
  driver()->DispatchMessageForTesting(kTestAppID1, msg1);

  task_environment().FastForwardBy(base::Minutes(1));
  IncomingMessage msg2;
  msg2.message_id = "msg_valid";
  driver()->DispatchMessageForTesting(kTestAppID1, msg2);

  // Fast forward by 1.5m (msg1 is now 2.5m old -> expired; msg2 is 1.5m old ->
  // valid).
  task_environment().FastForwardBy(base::Seconds(90));

  FakeGCMAppHandler handler;
  driver()->AddAppHandler(kTestAppID1, &handler);
  base::ScopedClosureRunner unregister_handler(base::BindOnce(
      &GCMDriver::RemoveAppHandler, base::Unretained(driver()), kTestAppID1));

  EXPECT_EQ(0u, driver()->GetBufferedMessagesCountForTesting(kTestAppID1));
  ASSERT_EQ(1u, handler.received_messages().size());
  EXPECT_EQ("msg_valid", handler.received_messages()[0].message.message_id);
}

TEST_F(GCMDriverBaseTest, BufferedMessages_BufferCapacityLimit_Enforced) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kGCMMessageBuffering);

  base::HistogramTester histogram_tester;

  // Buffer 10 messages for one app (cap is 5).
  constexpr size_t kTotalSent = 10;
  constexpr size_t kCap = 5;
  for (size_t i = 0; i < kTotalSent; ++i) {
    IncomingMessage msg;
    msg.message_id = base::StringPrintf("msg_%zu", i);
    driver()->DispatchMessageForTesting(kTestAppID1, msg);
  }

  EXPECT_EQ(kCap, driver()->GetBufferedMessagesCountForTesting(kTestAppID1));
  // 5 evicted messages recorded false in histogram.
  histogram_tester.ExpectBucketCount("GCM.DeliveredToAppHandler", false, 5);

  FakeGCMAppHandler handler;
  driver()->AddAppHandler(kTestAppID1, &handler);
  base::ScopedClosureRunner unregister_handler(base::BindOnce(
      &GCMDriver::RemoveAppHandler, base::Unretained(driver()), kTestAppID1));

  ASSERT_EQ(kCap, handler.received_messages().size());
  // Verify oldest 5 messages (msg_0 to msg_4) were evicted; oldest remaining is
  // msg_5.
  EXPECT_EQ("msg_5", handler.received_messages()[0].message.message_id);
  EXPECT_EQ("msg_9", handler.received_messages()[kCap - 1].message.message_id);
  // 5 remaining messages delivered to handler.
  histogram_tester.ExpectBucketCount("GCM.DeliveredToAppHandler", true, 5);
  histogram_tester.ExpectBucketCount("GCM.DeliveredToAppHandler", false, 5);
}

TEST_F(GCMDriverBaseTest, BufferedMessagesExpiredAfterTTL) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kGCMMessageBuffering,
      {{"message_buffering_ttl_seconds", "120"}});
  IncomingMessage incoming_msg;
  incoming_msg.sender_id = "sender_123";
  incoming_msg.message_id = "msg_456";

  // Dispatch message when no AppHandler is registered.
  driver()->DispatchMessageForTesting(kTestAppID1, incoming_msg);
  EXPECT_EQ(1u, driver()->GetBufferedMessagesCountForTesting(kTestAppID1));

  // Fast forward past the 2-minute TTL.
  task_environment().FastForwardBy(base::Minutes(3));

  // Add AppHandler after TTL expiration; pruning occurs when handler is added.
  FakeGCMAppHandler handler;
  driver()->AddAppHandler(kTestAppID1, &handler);
  base::ScopedClosureRunner unregister_handler(base::BindOnce(
      &GCMDriver::RemoveAppHandler, base::Unretained(driver()), kTestAppID1));

  // Verify expired message was pruned and not delivered.
  EXPECT_EQ(0u, driver()->GetBufferedMessagesCountForTesting(kTestAppID1));
  EXPECT_TRUE(handler.received_messages().empty());
}

TEST_F(GCMDriverBaseTest, BufferedMessages_PruneUnrelatedAppOnAddAppHandler) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kGCMMessageBuffering,
      {{"message_buffering_ttl_seconds", "120"}});

  IncomingMessage msg1;
  msg1.message_id = "msg_app1";
  driver()->DispatchMessageForTesting(kTestAppID1, msg1);

  IncomingMessage msg2;
  msg2.message_id = "msg_app2";
  driver()->DispatchMessageForTesting(kTestAppID2, msg2);

  EXPECT_EQ(1u, driver()->GetBufferedMessagesCountForTesting(kTestAppID1));
  EXPECT_EQ(1u, driver()->GetBufferedMessagesCountForTesting(kTestAppID2));

  // Fast forward past TTL.
  task_environment().FastForwardBy(base::Minutes(3));

  // Adding handler for kTestAppID1 prunes expired messages for all apps
  // including kTestAppID2.
  FakeGCMAppHandler handler1;
  driver()->AddAppHandler(kTestAppID1, &handler1);
  base::ScopedClosureRunner unregister1(base::BindOnce(
      &GCMDriver::RemoveAppHandler, base::Unretained(driver()), kTestAppID1));

  EXPECT_EQ(0u, driver()->GetBufferedMessagesCountForTesting(kTestAppID1));
  EXPECT_EQ(0u, driver()->GetBufferedMessagesCountForTesting(kTestAppID2));
  EXPECT_TRUE(handler1.received_messages().empty());
}

TEST_F(GCMDriverBaseTest, GetGCMMessageBufferingTTL) {
  EXPECT_EQ(base::Seconds(45), features::GetGCMMessageBufferingTTL());

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kGCMMessageBuffering,
      {{"message_buffering_ttl_seconds", "120"}});
  EXPECT_EQ(base::Seconds(120), features::GetGCMMessageBufferingTTL());
}

TEST_F(GCMDriverBaseTest, BufferedMessages_CanHandlePrefixAppHandler) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kGCMMessageBuffering);

  // Dispatch messages for apps that match prefix "wp:".
  IncomingMessage msg1;
  msg1.message_id = "msg_wp1";
  driver()->DispatchMessageForTesting("wp:https://example.com#1", msg1);

  IncomingMessage msg2;
  msg2.message_id = "msg_wp2";
  driver()->DispatchMessageForTesting("wp:https://example.com#2", msg2);

  IncomingMessage msg_unrelated;
  msg_unrelated.message_id = "msg_other";
  driver()->DispatchMessageForTesting("other_app", msg_unrelated);

  EXPECT_EQ(1u, driver()->GetBufferedMessagesCountForTesting(
                    "wp:https://example.com#1"));
  EXPECT_EQ(1u, driver()->GetBufferedMessagesCountForTesting(
                    "wp:https://example.com#2"));
  EXPECT_EQ(1u, driver()->GetBufferedMessagesCountForTesting("other_app"));

  // Register prefix handler for "wp:".
  PrefixTestGCMAppHandler wp_handler("wp:");
  driver()->AddAppHandler("wp:", &wp_handler);
  base::ScopedClosureRunner unregister(base::BindOnce(
      &GCMDriver::RemoveAppHandler, base::Unretained(driver()), "wp:"));

  // Verify matching prefix messages are delivered and cleared from buffer.
  EXPECT_EQ(0u, driver()->GetBufferedMessagesCountForTesting(
                    "wp:https://example.com#1"));
  EXPECT_EQ(0u, driver()->GetBufferedMessagesCountForTesting(
                    "wp:https://example.com#2"));
  EXPECT_EQ(1u, driver()->GetBufferedMessagesCountForTesting("other_app"));

  ASSERT_EQ(2u, wp_handler.received_messages().size());
  EXPECT_EQ("wp:https://example.com#1",
            wp_handler.received_messages()[0].app_id);
  EXPECT_EQ("msg_wp1", wp_handler.received_messages()[0].message.message_id);
  EXPECT_EQ("wp:https://example.com#2",
            wp_handler.received_messages()[1].app_id);
  EXPECT_EQ("msg_wp2", wp_handler.received_messages()[1].message.message_id);
}

TEST_F(GCMDriverBaseTest, BufferedMessages_MaxAppsCapacityLimit_Enforced) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kGCMMessageBuffering,
      {{"message_buffering_ttl_seconds", "120"}});

  // Fill up buffer to 50 apps (kMaxBufferedApps).
  constexpr size_t kMaxApps = 50;
  for (size_t i = 0; i < kMaxApps; ++i) {
    IncomingMessage msg;
    msg.message_id = base::StringPrintf("msg_%zu", i);
    driver()->DispatchMessageForTesting(base::StringPrintf("app_%zu", i), msg);
  }

  // 51st app should be dropped when all 50 apps are still valid (not expired).
  IncomingMessage msg_dropped;
  msg_dropped.message_id = "msg_dropped";
  driver()->DispatchMessageForTesting("app_50", msg_dropped);
  EXPECT_EQ(0u, driver()->GetBufferedMessagesCountForTesting("app_50"));

  // Fast-forward past TTL to expire all 50 apps.
  task_environment().FastForwardBy(base::Minutes(3));

  // Sending message for a new app now should prune expired apps and succeed.
  IncomingMessage msg_new;
  msg_new.message_id = "msg_new";
  driver()->DispatchMessageForTesting("app_new", msg_new);
  EXPECT_EQ(1u, driver()->GetBufferedMessagesCountForTesting("app_new"));
}

TEST_F(GCMDriverBaseTest, BufferedMessagesClearedOnShutdown) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kGCMMessageBuffering);

  base::HistogramTester histogram_tester;

  IncomingMessage msg;
  msg.message_id = "msg_shutdown";
  driver()->DispatchMessageForTesting(kTestAppID1, msg);
  EXPECT_EQ(1u, driver()->GetBufferedMessagesCountForTesting(kTestAppID1));
  histogram_tester.ExpectTotalCount("GCM.DeliveredToAppHandler", 0);

  driver()->Shutdown();
  EXPECT_EQ(0u, driver()->GetBufferedMessagesCountForTesting(kTestAppID1));
  histogram_tester.ExpectUniqueSample("GCM.DeliveredToAppHandler", false, 1);
}

TEST_F(GCMDriverBaseTest, BufferedMessages_TelemetryEmittedOnDeliveryAndDrop) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kGCMMessageBuffering,
      {{"message_buffering_ttl_seconds", "120"}});

  base::HistogramTester histogram_tester;

  // 1. Dispatch message when handler is missing -> buffered, no histogram yet.
  IncomingMessage msg1;
  msg1.message_id = "msg1";
  driver()->DispatchMessageForTesting(kTestAppID1, msg1);
  histogram_tester.ExpectTotalCount("GCM.DeliveredToAppHandler", 0);

  // 2. Add handler -> message is delivered, emits true.
  FakeGCMAppHandler handler1;
  driver()->AddAppHandler(kTestAppID1, &handler1);
  histogram_tester.ExpectUniqueSample("GCM.DeliveredToAppHandler", true, 1);
  driver()->RemoveAppHandler(kTestAppID1);

  // 3. Dispatch message and let TTL expire -> on next prune, emits false.
  IncomingMessage msg2;
  msg2.message_id = "msg2";
  driver()->DispatchMessageForTesting(kTestAppID1, msg2);
  task_environment().FastForwardBy(base::Minutes(3));

  FakeGCMAppHandler handler2;
  driver()->AddAppHandler(kTestAppID1, &handler2);
  histogram_tester.ExpectBucketCount("GCM.DeliveredToAppHandler", false, 1);
  driver()->RemoveAppHandler(kTestAppID1);
}

TEST_F(GCMDriverBaseTest, BufferedMessages_SyncAndFcmInvalidationsTelemetry) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kGCMMessageBuffering,
      {{"message_buffering_ttl_seconds", "120"}});

  base::HistogramTester histogram_tester;

  constexpr char kSyncAppId[] = "com.google.chrome.sync.invalidations";
  constexpr char kFcmAppId[] =
      "com.google.chrome.fcm.invalidations.permission.update";

  IncomingMessage sync_msg;
  sync_msg.message_id = "sync_msg_1";
  driver()->DispatchMessageForTesting(kSyncAppId, sync_msg);

  IncomingMessage fcm_msg;
  fcm_msg.message_id = "fcm_msg_1";
  driver()->DispatchMessageForTesting(kFcmAppId, fcm_msg);

  histogram_tester.ExpectTotalCount("GCM.DeliveredToAppHandler", 0);
  histogram_tester.ExpectTotalCount(
      "GCM.DeliveredToAppHandler.SyncInvalidations", 0);
  histogram_tester.ExpectTotalCount(
      "GCM.DeliveredToAppHandler.FcmInvalidations", 0);

  // Deliver sync message upon handler registration.
  {
    FakeGCMAppHandler sync_handler;
    driver()->AddAppHandler(kSyncAppId, &sync_handler);
    base::ScopedClosureRunner unregister_sync(base::BindOnce(
        &GCMDriver::RemoveAppHandler, base::Unretained(driver()), kSyncAppId));
    histogram_tester.ExpectUniqueSample(
        "GCM.DeliveredToAppHandler.SyncInvalidations", true, 1);
    ASSERT_EQ(1u, sync_handler.received_messages().size());
  }

  // Deliver FCM message upon handler registration.
  {
    FakeGCMAppHandler fcm_handler;
    driver()->AddAppHandler(kFcmAppId, &fcm_handler);
    base::ScopedClosureRunner unregister_fcm(base::BindOnce(
        &GCMDriver::RemoveAppHandler, base::Unretained(driver()), kFcmAppId));
    histogram_tester.ExpectUniqueSample(
        "GCM.DeliveredToAppHandler.FcmInvalidations", true, 1);
    ASSERT_EQ(1u, fcm_handler.received_messages().size());
  }

  // Receive another FCM message and let it expire via TTL to test drop
  // telemetry.
  IncomingMessage fcm_msg_dropped;
  fcm_msg_dropped.message_id = "fcm_msg_dropped";
  driver()->DispatchMessageForTesting(kFcmAppId, fcm_msg_dropped);
  task_environment().FastForwardBy(base::Minutes(3));

  {
    FakeGCMAppHandler fcm_handler2;
    driver()->AddAppHandler(kFcmAppId, &fcm_handler2);
    base::ScopedClosureRunner unregister_fcm2(base::BindOnce(
        &GCMDriver::RemoveAppHandler, base::Unretained(driver()), kFcmAppId));
    histogram_tester.ExpectBucketCount(
        "GCM.DeliveredToAppHandler.FcmInvalidations", false, 1);
  }
}

}  // namespace gcm
