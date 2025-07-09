// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/android/comments/comments_service_bridge.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/test/task_environment.h"
#include "components/collaboration/public/comments/comments_service.h"
#include "components/collaboration/test_support/mock_comments_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/collaboration/internal/comments_native_j_unittests_jni_headers/CommentsServiceBridgeUnitTestCompanion_jni.h"

using testing::Return;

namespace collaboration::comments::android {

class CommentsServiceBridgeTest : public testing::Test {
 public:
  CommentsServiceBridgeTest() = default;
  ~CommentsServiceBridgeTest() override = default;

  void SetUp() override {
    bridge_ = CommentsServiceBridge::CreateForTest(&service());
    j_service_ = bridge_->GetJavaObject();
    j_companion_ = Java_CommentsServiceBridgeUnitTestCompanion_Constructor(
        base::android::AttachCurrentThread(), j_service_);
  }

  // Member accessors.
  CommentsServiceBridge* bridge() { return bridge_.get(); }
  MockCommentsService& service() { return service_; }
  base::android::ScopedJavaGlobalRef<jobject> j_companion() {
    return j_companion_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  MockCommentsService service_;
  std::unique_ptr<CommentsServiceBridge> bridge_;
  base::android::ScopedJavaGlobalRef<jobject> j_service_;
  base::android::ScopedJavaGlobalRef<jobject> j_companion_;
};

TEST_F(CommentsServiceBridgeTest, TestIsInitialized) {
  EXPECT_CALL(service(), IsInitialized()).WillOnce(Return(false));
  EXPECT_FALSE(Java_CommentsServiceBridgeUnitTestCompanion_isInitialized(
      base::android::AttachCurrentThread(), j_companion()));

  EXPECT_CALL(service(), IsInitialized()).WillOnce(Return(true));
  EXPECT_TRUE(Java_CommentsServiceBridgeUnitTestCompanion_isInitialized(
      base::android::AttachCurrentThread(), j_companion()));
}

TEST_F(CommentsServiceBridgeTest, TestIsEmptyService) {
  EXPECT_CALL(service(), IsEmptyService()).WillOnce(Return(false));
  EXPECT_FALSE(Java_CommentsServiceBridgeUnitTestCompanion_isEmptyService(
      base::android::AttachCurrentThread(), j_companion()));

  EXPECT_CALL(service(), IsEmptyService()).WillOnce(Return(true));
  EXPECT_TRUE(Java_CommentsServiceBridgeUnitTestCompanion_isEmptyService(
      base::android::AttachCurrentThread(), j_companion()));
}

}  // namespace collaboration::comments::android
