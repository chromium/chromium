// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/environment_integrity/android/android_environment_integrity_service.h"

#include <cstdint>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/environment_integrity/android/android_environment_integrity_data_manager.h"
#include "components/environment_integrity/android/integrity_service_bridge.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/environment_integrity/environment_integrity_service.mojom.h"

namespace environment_integrity {

namespace {

const char kTestOrigin[] = "https://test.com";
const int64_t kHandle = 123456789;
const int64_t kHandleSecondary = 987654321;
const std::vector<uint8_t> kContentBinding = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};

const std::vector<uint8_t> kToken = {1, 2, 3, 4};

}  // namespace

using HandleGenerator = base::RepeatingCallback<HandleCreationResult()>;
using TokenGenerator =
    base::RepeatingCallback<GetTokenResult(int64_t,
                                           const std::vector<uint8_t>&)>;

class TestIntegrityService : public IntegrityService {
 public:
  TestIntegrityService(HandleGenerator handle_generator,
                       TokenGenerator token_generator)
      : handle_generator_(std::move(handle_generator)),
        token_generator_(std::move(token_generator)) {}

  ~TestIntegrityService() override = default;
  bool IsIntegrityAvailable() override { return true; }

  void CreateIntegrityHandle(CreateHandleCallback callback) override {
    std::move(callback).Run(handle_generator_.Run());
  }

  void GetEnvironmentIntegrityToken(int64_t handle,
                                    const std::vector<uint8_t>& content_binding,
                                    GetTokenCallback callback) override {
    std::move(callback).Run(token_generator_.Run(handle, content_binding));
  }

 private:
  HandleGenerator handle_generator_;
  TokenGenerator token_generator_;
};

class BaseAndroidEnvironmentIntegrityServiceTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    GURL url = GURL(kTestOrigin);
    content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);
  }

  void CreateService(HandleGenerator handle_generator,
                     TokenGenerator token_generator) {
    AndroidEnvironmentIntegrityService::CreateForTest(
        main_rfh(), remote_.BindNewPipeAndPassReceiver(),
        std::make_unique<TestIntegrityService>(std::move(handle_generator),
                                               std::move(token_generator)));
  }

  AndroidEnvironmentIntegrityDataManager* GetDataManager() {
    content::StoragePartition* storage_partition =
        main_rfh()->GetStoragePartition();
    return AndroidEnvironmentIntegrityDataManager::
        GetOrCreateForStoragePartition(storage_partition);
  }

  void StoreHandleForOrigin(base::StringPiece origin_str, int64_t handle) {
    AndroidEnvironmentIntegrityDataManager* data_manager = GetDataManager();
    url::Origin origin = url::Origin::Create(GURL(origin_str));
    data_manager->SetHandle(origin, kHandle);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  mojo::Remote<blink::mojom::EnvironmentIntegrityService> remote_;
};

class AndroidEnvironmentIntegrityServiceTest
    : public BaseAndroidEnvironmentIntegrityServiceTest {
 public:
  void SetUp() override {
    BaseAndroidEnvironmentIntegrityServiceTest::SetUp();
    feature_list_.InitAndEnableFeature(
        blink::features::kWebEnvironmentIntegrity);
  }
};

TEST_F(AndroidEnvironmentIntegrityServiceTest,
       IntegrityCanBeCalledReturnsGeneratedTokenAndPersistsHandle) {
  HandleGenerator handle_generator =
      base::BindLambdaForTesting([]() -> HandleCreationResult {
        return {.response_code = IntegrityResponse::kSuccess,
                .handle = kHandle};
      });

  TokenGenerator token_generator = base::BindLambdaForTesting(
      [](int64_t handle, const std::vector<uint8_t>& content_binding) {
        EXPECT_EQ(kHandle, handle);
        EXPECT_EQ(kContentBinding, content_binding);
        GetTokenResult result;
        result.response_code = IntegrityResponse::kSuccess;
        result.token = kToken;
        return result;
      });
  CreateService(std::move(handle_generator), std::move(token_generator));

  remote_->GetEnvironmentIntegrity(
      kContentBinding,
      base::BindLambdaForTesting(
          [](blink::mojom::EnvironmentIntegrityResponseCode code,
             const std::vector<uint8_t>& token) {
            EXPECT_EQ(blink::mojom::EnvironmentIntegrityResponseCode::kSuccess,
                      code);
            EXPECT_EQ(kToken, token);
          }));

  task_environment()->RunUntilIdle();

  // Assert that we stored the handle in the data manager.
  AndroidEnvironmentIntegrityDataManager* data_manager = GetDataManager();
  data_manager->GetHandle(
      url::Origin::Create(GURL(kTestOrigin)),
      base::BindLambdaForTesting([](absl::optional<int64_t> maybe_handle) {
        ASSERT_TRUE(maybe_handle);
        EXPECT_EQ(kHandle, *maybe_handle);
      }));
  task_environment()->RunUntilIdle();
}

TEST_F(AndroidEnvironmentIntegrityServiceTest,
       FailWithInternalErrorIfHandleGenerationErrors) {
  HandleGenerator handle_generator =
      base::BindLambdaForTesting([]() -> HandleCreationResult {
        return {.response_code = IntegrityResponse::kUnknownError,
                .error_message = "error"};
      });

  TokenGenerator token_generator = base::BindLambdaForTesting(
      [](int64_t handle, const std::vector<uint8_t>& content_binding) {
        NOTREACHED() << "No request for token should be made";
        GetTokenResult result;
        return result;
      });
  CreateService(std::move(handle_generator), std::move(token_generator));

  remote_->GetEnvironmentIntegrity(
      kContentBinding,
      base::BindLambdaForTesting(
          [](blink::mojom::EnvironmentIntegrityResponseCode code,
             const std::vector<uint8_t>& token) {
            EXPECT_EQ(
                blink::mojom::EnvironmentIntegrityResponseCode::kInternalError,
                code);
            EXPECT_EQ(0u, token.size());
          }));

  task_environment()->RunUntilIdle();
}

TEST_F(AndroidEnvironmentIntegrityServiceTest,
       FailWithTimeoutErrorIfHandleGenerationTimesOut) {
  HandleGenerator handle_generator =
      base::BindLambdaForTesting([]() -> HandleCreationResult {
        return {.response_code = IntegrityResponse::kTimeout,
                .error_message = "error"};
      });

  TokenGenerator token_generator = base::BindLambdaForTesting(
      [](int64_t handle, const std::vector<uint8_t>& content_binding) {
        NOTREACHED() << "No request for token should be made";
        GetTokenResult result;
        return result;
      });
  CreateService(std::move(handle_generator), std::move(token_generator));

  remote_->GetEnvironmentIntegrity(
      kContentBinding,
      base::BindLambdaForTesting(
          [](blink::mojom::EnvironmentIntegrityResponseCode code,
             const std::vector<uint8_t>& token) {
            EXPECT_EQ(blink::mojom::EnvironmentIntegrityResponseCode::kTimeout,
                      code);
            EXPECT_EQ(0u, token.size());
          }));

  task_environment()->RunUntilIdle();
}

TEST_F(AndroidEnvironmentIntegrityServiceTest,
       FailWithInternalErrorIfTokenGenerationErrors) {
  HandleGenerator handle_generator =
      base::BindLambdaForTesting([]() -> HandleCreationResult {
        return {.response_code = IntegrityResponse::kSuccess,
                .handle = kHandle};
      });

  TokenGenerator token_generator = base::BindLambdaForTesting(
      [](int64_t handle, const std::vector<uint8_t>& content_binding) {
        GetTokenResult result;
        result.response_code = IntegrityResponse::kUnknownError;
        result.error_message = "error";
        return result;
      });
  CreateService(std::move(handle_generator), std::move(token_generator));

  remote_->GetEnvironmentIntegrity(
      kContentBinding,
      base::BindLambdaForTesting(
          [](blink::mojom::EnvironmentIntegrityResponseCode code,
             const std::vector<uint8_t>& token) {
            EXPECT_EQ(
                blink::mojom::EnvironmentIntegrityResponseCode::kInternalError,
                code);
            EXPECT_EQ(0u, token.size());
          }));

  task_environment()->RunUntilIdle();
}

TEST_F(AndroidEnvironmentIntegrityServiceTest,
       FailWithTimeoutErrorIfTokenGenerationTimesOut) {
  HandleGenerator handle_generator =
      base::BindLambdaForTesting([]() -> HandleCreationResult {
        return {.response_code = IntegrityResponse::kSuccess,
                .handle = kHandle};
      });

  TokenGenerator token_generator = base::BindLambdaForTesting(
      [](int64_t handle, const std::vector<uint8_t>& content_binding) {
        GetTokenResult result;
        result.response_code = IntegrityResponse::kTimeout;
        result.error_message = "error";
        return result;
      });
  CreateService(std::move(handle_generator), std::move(token_generator));

  remote_->GetEnvironmentIntegrity(
      kContentBinding,
      base::BindLambdaForTesting(
          [](blink::mojom::EnvironmentIntegrityResponseCode code,
             const std::vector<uint8_t>& token) {
            EXPECT_EQ(blink::mojom::EnvironmentIntegrityResponseCode::kTimeout,
                      code);
            EXPECT_EQ(0u, token.size());
          }));

  task_environment()->RunUntilIdle();
}

TEST_F(AndroidEnvironmentIntegrityServiceTest, ReuseStoredHandle) {
  StoreHandleForOrigin(kTestOrigin, kHandle);

  HandleGenerator handle_generator =
      base::BindLambdaForTesting([]() -> HandleCreationResult {
        NOTREACHED() << "Not expected to be called as we already have a handle";
        return {};
      });

  TokenGenerator token_generator = base::BindLambdaForTesting(
      [](int64_t handle, const std::vector<uint8_t>& content_binding) {
        EXPECT_EQ(kContentBinding, content_binding);
        EXPECT_EQ(kHandle, handle);
        GetTokenResult result;
        result.response_code = IntegrityResponse::kSuccess;
        result.token = kToken;
        return result;
      });
  CreateService(std::move(handle_generator), std::move(token_generator));

  remote_->GetEnvironmentIntegrity(
      kContentBinding,
      base::BindLambdaForTesting(
          [](blink::mojom::EnvironmentIntegrityResponseCode code,
             const std::vector<uint8_t>& token) {
            EXPECT_EQ(blink::mojom::EnvironmentIntegrityResponseCode::kSuccess,
                      code);
            EXPECT_EQ(kToken, token);
          }));

  task_environment()->RunUntilIdle();
}

TEST_F(AndroidEnvironmentIntegrityServiceTest, SucceedIfHandleIsTooOld) {
  int32_t handle_generation_attempts = 0;
  HandleGenerator handle_generator = base::BindLambdaForTesting(
      [&handle_generation_attempts]() -> HandleCreationResult {
        ++handle_generation_attempts;
        if (handle_generation_attempts == 1) {
          return {.response_code = IntegrityResponse::kSuccess,
                  .handle = kHandle};
        } else {
          return {.response_code = IntegrityResponse::kSuccess,
                  .handle = kHandleSecondary};
        }
      });

  int32_t token_generation_attempts = 0;
  TokenGenerator token_generator = base::BindLambdaForTesting(
      [&token_generation_attempts](
          int64_t handle, const std::vector<uint8_t>& content_binding) {
        EXPECT_EQ(kContentBinding, content_binding);
        ++token_generation_attempts;
        GetTokenResult result;
        if (handle == kHandle) {
          result.response_code = IntegrityResponse::kInvalidHandle;
          result.error_message = "error";
        } else {
          EXPECT_EQ(kHandleSecondary, handle);
          result.response_code = IntegrityResponse::kSuccess;
          result.token = kToken;
        }
        return result;
      });
  CreateService(std::move(handle_generator), std::move(token_generator));

  remote_->GetEnvironmentIntegrity(
      kContentBinding,
      base::BindLambdaForTesting(
          [](blink::mojom::EnvironmentIntegrityResponseCode code,
             const std::vector<uint8_t>& token) {
            EXPECT_EQ(blink::mojom::EnvironmentIntegrityResponseCode::kSuccess,
                      code);
            EXPECT_EQ(kToken, token);
          }));

  task_environment()->RunUntilIdle();
  EXPECT_EQ(2, handle_generation_attempts);
  EXPECT_EQ(2, token_generation_attempts);
}

TEST_F(AndroidEnvironmentIntegrityServiceTest, SucceedIfStoredHandleIsTooOld) {
  StoreHandleForOrigin(kTestOrigin, kHandle);

  int32_t handle_generation_attempts = 0;
  HandleGenerator handle_generator = base::BindLambdaForTesting(
      [&handle_generation_attempts]() -> HandleCreationResult {
        ++handle_generation_attempts;
        return {.response_code = IntegrityResponse::kSuccess,
                .handle = kHandleSecondary};
      });

  int32_t token_generation_attempts = 0;
  // Respond that the original kHandle is too old, but any new handle is fine.
  TokenGenerator token_generator = base::BindLambdaForTesting(
      [&token_generation_attempts](
          int64_t handle, const std::vector<uint8_t>& content_binding) {
        EXPECT_EQ(kContentBinding, content_binding);
        ++token_generation_attempts;
        GetTokenResult result;
        if (handle == kHandle) {
          result.response_code = IntegrityResponse::kInvalidHandle;
          result.error_message = "error";
        } else {
          EXPECT_EQ(kHandleSecondary, handle);
          result.response_code = IntegrityResponse::kSuccess;
          result.token = kToken;
        }
        return result;
      });
  CreateService(std::move(handle_generator), std::move(token_generator));

  remote_->GetEnvironmentIntegrity(
      kContentBinding,
      base::BindLambdaForTesting(
          [](blink::mojom::EnvironmentIntegrityResponseCode code,
             const std::vector<uint8_t>& token) {
            EXPECT_EQ(blink::mojom::EnvironmentIntegrityResponseCode::kSuccess,
                      code);
            EXPECT_EQ(kToken, token);
          }));

  task_environment()->RunUntilIdle();
  // We expect that the stored handle was attempted used first.
  EXPECT_EQ(1, handle_generation_attempts);
  EXPECT_EQ(2, token_generation_attempts);
}

TEST_F(AndroidEnvironmentIntegrityServiceTest, HandleRecreationOnlyTriedOnce) {
  // Store a handle in the storage partition before the test
  content::StoragePartition* storage_partition =
      main_rfh()->GetStoragePartition();
  AndroidEnvironmentIntegrityDataManager* data_manager =
      AndroidEnvironmentIntegrityDataManager::GetOrCreateForStoragePartition(
          storage_partition);
  url::Origin origin = url::Origin::Create(GURL(kTestOrigin));
  data_manager->SetHandle(origin, kHandle);

  int32_t handle_generation_attempts = 0;
  HandleGenerator handle_generator = base::BindLambdaForTesting(
      [&handle_generation_attempts]() -> HandleCreationResult {
        ++handle_generation_attempts;
        if (handle_generation_attempts > 1) {
          NOTREACHED() << "Only create 1 new handle.";
        }
        return {.response_code = IntegrityResponse::kSuccess,
                .handle = kHandleSecondary};
      });

  // Always respond the handle is too old.
  int32_t token_generation_attempts = 0;
  TokenGenerator token_generator = base::BindLambdaForTesting(
      [&token_generation_attempts](
          int64_t handle, const std::vector<uint8_t>& content_binding) {
        ++token_generation_attempts;
        if (token_generation_attempts > 2) {
          NOTREACHED() << "We only expected two token generation requests.";
        }
        GetTokenResult result;
        result.response_code = IntegrityResponse::kInvalidHandle;
        result.error_message = "error";
        return result;
      });

  CreateService(std::move(handle_generator), std::move(token_generator));

  remote_->GetEnvironmentIntegrity(
      kContentBinding,
      base::BindLambdaForTesting(
          [](blink::mojom::EnvironmentIntegrityResponseCode code,
             const std::vector<uint8_t>& token) {
            EXPECT_EQ(
                blink::mojom::EnvironmentIntegrityResponseCode::kInternalError,
                code);
            EXPECT_EQ(0u, token.size());
          }));

  task_environment()->RunUntilIdle();
  // We expect that the stored handle was attempted used first.
  EXPECT_EQ(1, handle_generation_attempts);
  // We only expect that they tried to create a token twice, once with the old
  // and once with the new handle.
  EXPECT_EQ(2, token_generation_attempts);
}

// Tests for when the feature flag is disabled - the service should simply error
// out.
class AndroidEnvironmentIntegrityServiceDisabledFeatureTest
    : public BaseAndroidEnvironmentIntegrityServiceTest {
 public:
  void SetUp() override {
    BaseAndroidEnvironmentIntegrityServiceTest::SetUp();
    feature_list_.InitAndDisableFeature(
        blink::features::kWebEnvironmentIntegrity);
  }
};

TEST_F(AndroidEnvironmentIntegrityServiceDisabledFeatureTest,
       ErrorIfFeatureIsDisabled) {
  HandleGenerator handle_generator =
      base::BindLambdaForTesting([]() -> HandleCreationResult {
        EXPECT_TRUE(false) << "No calls expected when feature is disabled";
        return {};
      });

  TokenGenerator token_generator = base::BindLambdaForTesting(
      [](int64_t handle, const std::vector<uint8_t>& content_binding) {
        EXPECT_TRUE(false) << "No calls expected when feature is disabled";
        GetTokenResult result;
        return result;
      });
  CreateService(std::move(handle_generator), std::move(token_generator));

  remote_->GetEnvironmentIntegrity(
      kContentBinding,
      base::BindLambdaForTesting(
          [](blink::mojom::EnvironmentIntegrityResponseCode code,
             const std::vector<uint8_t>& token) {
            EXPECT_EQ(
                blink::mojom::EnvironmentIntegrityResponseCode::kInternalError,
                code);
          }));

  task_environment()->RunUntilIdle();
}

}  // namespace environment_integrity
