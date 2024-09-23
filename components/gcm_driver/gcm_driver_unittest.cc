// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/base64.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "components/gcm_driver/crypto/gcm_decryption_result.h"
#include "components/gcm_driver/crypto/gcm_encryption_provider.h"
#include "components/gcm_driver/crypto/gcm_encryption_result.h"
#include "components/gcm_driver/fake_gcm_client_factory.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_driver_desktop.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "crypto/ec_private_key.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const char kTestAppID1[] = "TestApp1";

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

 private:
  base::ScopedTempDir temp_dir_;
  TestingPrefServiceSimple prefs_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
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

GCMDriverBaseTest::GCMDriverBaseTest() : io_thread_("IOThread") {}

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
      io_thread_.task_runner(), task_environment_.GetMainThreadTaskRunner());
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

}  // namespace gcm
