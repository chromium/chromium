// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_loader.h"

#include "base/check.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unexportable_keys {

namespace {

constexpr crypto::SignatureVerifier::SignatureAlgorithm
    kAcceptableAlgorithms[] = {crypto::SignatureVerifier::ECDSA_SHA256};
constexpr BackgroundTaskPriority kTaskPriority =
    BackgroundTaskPriority::kUserVisible;

}  // namespace

class UnexportableKeyLoaderTest : public testing::Test {
 public:
  UnexportableKeyLoaderTest()
      : task_manager_(std::make_unique<UnexportableKeyTaskManager>(
            crypto::UnexportableKeyProvider::Config())),
        service_(std::make_unique<UnexportableKeyServiceImpl>(*task_manager_)) {
  }

  UnexportableKeyServiceImpl& service() { return *service_; }

  void RunBackgroundTasks() { task_environment_.RunUntilIdle(); }

  void ResetService() {
    task_manager_ = std::make_unique<UnexportableKeyTaskManager>(
        crypto::UnexportableKeyProvider::Config());
    service_ = std::make_unique<UnexportableKeyServiceImpl>(*task_manager_);
  }

  void DisableKeyProvider() {
    // Using `emplace()` to destroy the existing scoped object before
    // constructing a new one.
    scoped_key_provider_.emplace<crypto::ScopedNullUnexportableKeyProvider>();
  }

  std::vector<uint8_t> GenerateNewKeyAndReturnWrappedKey() {
    base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
    service().GenerateSigningKeySlowlyAsync(
        kAcceptableAlgorithms, kTaskPriority, generate_future.GetCallback());
    RunBackgroundTasks();
    ServiceErrorOr<UnexportableKeyId> key_id = generate_future.Get();
    CHECK(key_id.has_value());

    ServiceErrorOr<std::vector<uint8_t>> wrapped_key =
        service().GetWrappedKey(*key_id);
    CHECK(wrapped_key.has_value());
    return *wrapped_key;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadPoolExecutionMode::
          QUEUED};  // QUEUED - tasks don't run until `RunUntilIdle()` is
                    // called.
  // Provides a mock key provider by default.
  absl::variant<crypto::ScopedMockUnexportableKeyProvider,
                crypto::ScopedNullUnexportableKeyProvider>
      scoped_key_provider_;
  std::unique_ptr<UnexportableKeyTaskManager> task_manager_;
  std::unique_ptr<UnexportableKeyServiceImpl> service_;
};

TEST_F(UnexportableKeyLoaderTest, CreateFromWrappedKeySync) {
  std::vector<uint8_t> wrapped_key = GenerateNewKeyAndReturnWrappedKey();

  // `wrapped_key` is already registered in the service. The loader should
  // return a key immediately.
  auto key_loader = UnexportableKeyLoader::CreateFromWrappedKey(
      service(), wrapped_key, kTaskPriority);
  EXPECT_EQ(key_loader->GetStateForTesting(),
            UnexportableKeyLoader::State::kReady);
  EXPECT_TRUE(key_loader->GetKeyIdOrError().has_value());

  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> on_load_future;
  key_loader->InvokeCallbackAfterKeyLoaded(on_load_future.GetCallback());
  EXPECT_TRUE(on_load_future.IsReady());
  EXPECT_EQ(key_loader->GetKeyIdOrError(), on_load_future.Get());
}

TEST_F(UnexportableKeyLoaderTest, CreateFromWrappedKeyAsync) {
  std::vector<uint8_t> wrapped_key = GenerateNewKeyAndReturnWrappedKey();
  // A new key is still registered inside the service. Reset the service to
  // remove the key.
  ResetService();

  auto key_loader = UnexportableKeyLoader::CreateFromWrappedKey(
      service(), wrapped_key, kTaskPriority);
  EXPECT_EQ(key_loader->GetStateForTesting(),
            UnexportableKeyLoader::State::kLoading);
  EXPECT_EQ(key_loader->GetKeyIdOrError(),
            base::unexpected(ServiceError::kKeyNotReady));

  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> on_load_future;
  key_loader->InvokeCallbackAfterKeyLoaded(on_load_future.GetCallback());
  EXPECT_FALSE(on_load_future.IsReady());

  RunBackgroundTasks();
  EXPECT_EQ(key_loader->GetStateForTesting(),
            UnexportableKeyLoader::State::kReady);
  EXPECT_TRUE(key_loader->GetKeyIdOrError().has_value());
  EXPECT_TRUE(on_load_future.IsReady());
  EXPECT_EQ(key_loader->GetKeyIdOrError(), on_load_future.Get());
}

TEST_F(UnexportableKeyLoaderTest, CreateFromWrappedKeyMultipleCallbacks) {
  std::vector<uint8_t> wrapped_key = GenerateNewKeyAndReturnWrappedKey();
  // A new key is still registered inside the service. Reset the service to
  // remove the key.
  ResetService();

  auto key_loader = UnexportableKeyLoader::CreateFromWrappedKey(
      service(), wrapped_key, kTaskPriority);

  std::array<base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>>, 5>
      on_load_futures;
  for (auto& future : on_load_futures) {
    key_loader->InvokeCallbackAfterKeyLoaded(future.GetCallback());
    EXPECT_FALSE(future.IsReady());
  }

  RunBackgroundTasks();
  EXPECT_EQ(key_loader->GetStateForTesting(),
            UnexportableKeyLoader::State::kReady);
  EXPECT_TRUE(key_loader->GetKeyIdOrError().has_value());
  for (auto& future : on_load_futures) {
    EXPECT_TRUE(future.IsReady());
    EXPECT_EQ(key_loader->GetKeyIdOrError(), future.Get());
  }
}

TEST_F(UnexportableKeyLoaderTest, CreateWithNewKey) {
  auto key_loader = UnexportableKeyLoader::CreateWithNewKey(
      service(), kAcceptableAlgorithms, kTaskPriority);
  EXPECT_EQ(key_loader->GetStateForTesting(),
            UnexportableKeyLoader::State::kLoading);
  EXPECT_EQ(key_loader->GetKeyIdOrError(),
            base::unexpected(ServiceError::kKeyNotReady));

  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> on_load_future;
  key_loader->InvokeCallbackAfterKeyLoaded(on_load_future.GetCallback());
  EXPECT_FALSE(on_load_future.IsReady());

  RunBackgroundTasks();
  EXPECT_EQ(key_loader->GetStateForTesting(),
            UnexportableKeyLoader::State::kReady);
  EXPECT_TRUE(key_loader->GetKeyIdOrError().has_value());
  EXPECT_TRUE(on_load_future.IsReady());
  EXPECT_EQ(key_loader->GetKeyIdOrError(), on_load_future.Get());
}

TEST_F(UnexportableKeyLoaderTest, CreateWithNewKeyFailure) {
  DisableKeyProvider();
  auto key_loader = UnexportableKeyLoader::CreateWithNewKey(
      service(), kAcceptableAlgorithms, kTaskPriority);
  EXPECT_EQ(key_loader->GetStateForTesting(),
            UnexportableKeyLoader::State::kReady);
  EXPECT_EQ(key_loader->GetKeyIdOrError(),
            base::unexpected(ServiceError::kNoKeyProvider));
}

TEST_F(UnexportableKeyLoaderTest, SignDataAfterLoading) {
  auto key_loader = UnexportableKeyLoader::CreateWithNewKey(
      service(), kAcceptableAlgorithms, kTaskPriority);

  base::test::TestFuture<ServiceErrorOr<std::vector<uint8_t>>> sign_future;
  key_loader->InvokeCallbackAfterKeyLoaded(base::BindLambdaForTesting(
      [&](ServiceErrorOr<UnexportableKeyId> key_id_or_error) {
        ASSERT_TRUE(key_id_or_error.has_value());
        service().SignSlowlyAsync(*key_id_or_error,
                                  std::vector<uint8_t>({1, 2, 3}),
                                  kTaskPriority, sign_future.GetCallback());
      }));
  EXPECT_FALSE(sign_future.IsReady());
  RunBackgroundTasks();
  EXPECT_TRUE(sign_future.IsReady());
  EXPECT_TRUE(sign_future.Get().has_value());
}

}  // namespace unexportable_keys
