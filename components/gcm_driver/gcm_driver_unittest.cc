// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_driver_desktop.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/gcm_driver/crypto/gcm_decryption_result.h"
#include "components/gcm_driver/crypto/gcm_encryption_provider.h"
#include "components/gcm_driver/fake_gcm_client_factory.h"
#include "components/gcm_driver/gcm_channel_status_request.h"
#include "components/gcm_driver/gcm_channel_status_syncer.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "crypto/ec_private_key.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const char kTestChannelStatusRequestURL[] = "http://channel.status.request.com";
const char kTestAppID1[] = "TestApp1";

// PKCS #8 encoded P-256 private key.
const char kPrivateKey[] =
    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgS8wRbDOWz0lKExvIVQiRKtPAP8"
    "dgHUHAw5gyOd5d4jKhRANCAARZb49Va5MD/KcWtc0oiWc2e8njBDtQzj0mzcOl1fDSt16Pvu6p"
    "fTU3MTWnImDNnkPxtXm58K7Uax8jFxA4TeXJ";
const char kFCMToken[] = "fcm_token";

void PumpCurrentLoop() {
  base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
}

}  // namespace

class GCMDriverBaseTest : public testing::Test {
 public:
  enum WaitToFinish { DO_NOT_WAIT, WAIT };

  GCMDriverBaseTest();
  ~GCMDriverBaseTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  GCMDriverDesktop* driver() { return driver_.get(); }
  SendWebPushMessageResult send_web_push_message_result() const {
    return send_web_push_message_result_;
  }
  base::Optional<std::string> send_web_push_message_id() const {
    return send_web_push_message_id_;
  }
  const std::string& send_web_push_message_payload() const {
    return send_web_push_message_payload_;
  }
  const std::string& p256dh() const { return p256dh_; }
  const std::string& auth_secret() const { return auth_secret_; }
  network::TestURLLoaderFactory& loader() { return test_url_loader_factory_; }
  GCMDecryptionResult decryption_result() { return decryption_result_; }
  const IncomingMessage& decrypted_message() { return decrypted_message_; }

  void PumpIOLoop();

  void CreateDriver();
  void ShutdownDriver();

  void SendWebPushMessage(const std::string& app_id,
                          WebPushMessage message,
                          base::Optional<net::HttpStatusCode> completion_status,
                          WaitToFinish wait_to_finish);
  void GetEncryptionInfo(const std::string& app_id,
                         WaitToFinish wait_to_finish);
  void DecryptMessage(const std::string& app_id,
                      IncomingMessage message,
                      WaitToFinish wait_to_finish);

  void SendWebPushMessageCompleted(SendWebPushMessageResult result,
                                   base::Optional<std::string> message_id);
  void GetEncryptionInfoCompleted(std::string p256dh, std::string auth_secret);
  void DecryptMessageCompleted(GCMDecryptionResult result,
                               const IncomingMessage& message);
  void UnregisterCompleted(GCMClient::Result result);

 private:
  base::ScopedTempDir temp_dir_;
  TestingPrefServiceSimple prefs_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::Thread io_thread_;
  network::TestURLLoaderFactory test_url_loader_factory_;

  std::unique_ptr<GCMDriverDesktop> driver_;

  base::Closure async_operation_completed_callback_;

  SendWebPushMessageResult send_web_push_message_result_;
  base::Optional<std::string> send_web_push_message_id_;
  std::string send_web_push_message_payload_;
  std::string p256dh_;
  std::string auth_secret_;

  GCMDecryptionResult decryption_result_ = GCMDecryptionResult::UNENCRYPTED;
  IncomingMessage decrypted_message_;

  DISALLOW_COPY_AND_ASSIGN(GCMDriverBaseTest);
};

GCMDriverBaseTest::GCMDriverBaseTest() : io_thread_("IOThread") {}

GCMDriverBaseTest::~GCMDriverBaseTest() = default;

void GCMDriverBaseTest::SetUp() {
  GCMChannelStatusSyncer::RegisterPrefs(prefs_.registry());
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
}

void GCMDriverBaseTest::PumpIOLoop() {
  base::RunLoop run_loop;
  io_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&PumpCurrentLoop), run_loop.QuitClosure());
  run_loop.Run();
}

void GCMDriverBaseTest::CreateDriver() {
  scoped_refptr<net::URLRequestContextGetter> request_context =
      new net::TestURLRequestContextGetter(io_thread_.task_runner());
  GCMClient::ChromeBuildInfo chrome_build_info;
  chrome_build_info.product_category_for_subtypes = "com.chrome.macosx";
  driver_ = std::make_unique<GCMDriverDesktop>(
      std::make_unique<FakeGCMClientFactory>(
          base::ThreadTaskRunnerHandle::Get(), io_thread_.task_runner()),
      chrome_build_info, kTestChannelStatusRequestURL, "user-agent-string",
      &prefs_, temp_dir_.GetPath(), base::DoNothing(),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_),
      network::TestNetworkConnectionTracker::GetInstance(),
      base::ThreadTaskRunnerHandle::Get(), io_thread_.task_runner(),
      task_environment_.GetMainThreadTaskRunner());
}

void GCMDriverBaseTest::ShutdownDriver() {
  driver()->Shutdown();
}

void GCMDriverBaseTest::SendWebPushMessage(
    const std::string& app_id,
    WebPushMessage message,
    base::Optional<net::HttpStatusCode> completion_status,
    WaitToFinish wait_to_finish) {
  std::string private_key_info;
  ASSERT_TRUE(base::Base64Decode(kPrivateKey, &private_key_info));
  std::unique_ptr<crypto::ECPrivateKey> private_key =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(std::vector<uint8_t>(
          private_key_info.begin(), private_key_info.end()));
  ASSERT_TRUE(private_key);

  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  driver_->SendWebPushMessage(
      app_id, /* authorized_entity= */ "", p256dh(), auth_secret(), kFCMToken,
      private_key.get(), std::move(message),
      base::BindOnce(&GCMDriverBaseTest::SendWebPushMessageCompleted,
                     base::Unretained(this)));

  if (completion_status) {
    ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
    network::TestURLLoaderFactory::PendingRequest* pendingRequest =
        loader().GetPendingRequest(0);
    const std::vector<network::DataElement>* body_elements =
        pendingRequest->request.request_body->elements();
    ASSERT_EQ(1UL, body_elements->size());
    const network::DataElement& body = body_elements->back();
    send_web_push_message_payload_ = std::string(body.bytes(), body.length());

    auto response_head = network::CreateURLResponseHead(*completion_status);
    response_head->headers->AddHeader(
        "location:https://fcm.googleapis.com/message_id");

    test_url_loader_factory_.SimulateResponseForPendingRequest(
        pendingRequest->request.url,
        network::URLLoaderCompletionStatus(net::OK), std::move(response_head),
        "");
  }

  if (wait_to_finish == WAIT)
    run_loop.Run();
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

void GCMDriverBaseTest::DecryptMessage(const std::string& app_id,
                                       IncomingMessage message,
                                       WaitToFinish wait_to_finish) {
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  driver()->GetEncryptionProviderInternal()->DecryptMessage(
      app_id, message,
      base::AdaptCallbackForRepeating(
          base::BindOnce(&GCMDriverBaseTest::DecryptMessageCompleted,
                         base::Unretained(this))));

  if (wait_to_finish == WAIT)
    run_loop.Run();
}

void GCMDriverBaseTest::SendWebPushMessageCompleted(
    SendWebPushMessageResult result,
    base::Optional<std::string> message_id) {
  send_web_push_message_result_ = result;
  send_web_push_message_id_ = message_id;
  if (!async_operation_completed_callback_.is_null())
    async_operation_completed_callback_.Run();
}

void GCMDriverBaseTest::GetEncryptionInfoCompleted(std::string p256dh,
                                                   std::string auth_secret) {
  p256dh_ = std::move(p256dh);
  auth_secret_ = std::move(auth_secret);
  if (!async_operation_completed_callback_.is_null())
    async_operation_completed_callback_.Run();
}

void GCMDriverBaseTest::DecryptMessageCompleted(
    GCMDecryptionResult result,
    const IncomingMessage& message) {
  decryption_result_ = result;
  decrypted_message_ = message;
  if (!async_operation_completed_callback_.is_null())
    async_operation_completed_callback_.Run();
}

// TODO(crbug.com/1009185): Test is failing on ASan build.
#if defined(ADDRESS_SANITIZER)
TEST_F(GCMDriverBaseTest, DISABLED_SendWebPushMessage) {
#else
TEST_F(GCMDriverBaseTest, SendWebPushMessage) {
#endif
  GetEncryptionInfo(kTestAppID1, GCMDriverBaseTest::WAIT);

  WebPushMessage message;
  message.time_to_live = 3600;
  message.payload = "payload";
  ASSERT_NO_FATAL_FAILURE(SendWebPushMessage(kTestAppID1, std::move(message),
                                             base::make_optional(net::HTTP_OK),
                                             GCMDriverBaseTest::WAIT));

  EXPECT_EQ(SendWebPushMessageResult::kSuccessful,
            send_web_push_message_result());
  EXPECT_EQ("message_id", send_web_push_message_id());

  IncomingMessage incoming_message;
  incoming_message.data["content-encoding"] = "aes128gcm";
  incoming_message.raw_data = send_web_push_message_payload();

  DecryptMessage(kTestAppID1, std::move(incoming_message),
                 GCMDriverBaseTest::WAIT);

  EXPECT_EQ(GCMDecryptionResult::DECRYPTED_DRAFT_08, decryption_result());
  EXPECT_EQ("payload", decrypted_message().raw_data);
}

TEST_F(GCMDriverBaseTest, SendWebPushMessageEncryptionError) {
  // Intentionally not creating encryption info.

  WebPushMessage message;
  message.time_to_live = 3600;
  message.payload = "payload";
  ASSERT_NO_FATAL_FAILURE(SendWebPushMessage(
      kTestAppID1, std::move(message), base::nullopt, GCMDriverBaseTest::WAIT));

  EXPECT_EQ(SendWebPushMessageResult::kEncryptionFailed,
            send_web_push_message_result());
  EXPECT_FALSE(send_web_push_message_id());
}

// TODO(crbug.com/1009185): Test is failing on ASan build.
#if defined(ADDRESS_SANITIZER)
TEST_F(GCMDriverBaseTest, DISABLED_SendWebPushMessageServerError) {
#else
TEST_F(GCMDriverBaseTest, SendWebPushMessageServerError) {
#endif
  GetEncryptionInfo(kTestAppID1, GCMDriverBaseTest::WAIT);

  WebPushMessage message;
  message.time_to_live = 3600;
  message.payload = "payload";
  ASSERT_NO_FATAL_FAILURE(
      SendWebPushMessage(kTestAppID1, std::move(message),
                         base::make_optional(net::HTTP_INTERNAL_SERVER_ERROR),
                         GCMDriverBaseTest::WAIT));

  EXPECT_EQ(SendWebPushMessageResult::kServerError,
            send_web_push_message_result());
  EXPECT_FALSE(send_web_push_message_id());
}

}  // namespace gcm
