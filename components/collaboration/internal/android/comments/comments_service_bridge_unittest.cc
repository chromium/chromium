// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/android/comments/comments_service_bridge.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/collaboration/public/comments/comments_service.h"
#include "components/collaboration/test_support/mock_comments_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/collaboration/internal/comments_native_j_unittests_jni_headers/CommentsServiceBridgeUnitTestCompanion_jni.h"

using ::testing::_;
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

TEST_F(CommentsServiceBridgeTest, TestAddComment) {
  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jstring> j_collab_id =
      base::android::ConvertUTF8ToJavaString(env, "collab_id_123");
  base::android::ScopedJavaLocalRef<jobject> j_gurl =
      url::GURLAndroid::FromNativeGURL(env, GURL("https://example.com"));
  base::android::ScopedJavaLocalRef<jstring> j_content =
      base::android::ConvertUTF8ToJavaString(env, "new comment content");
  base::android::ScopedJavaLocalRef<jstring> j_parent_id =
      base::android::ConvertUTF8ToJavaString(
          env, base::Uuid::GenerateRandomV4().AsLowercaseString());
  base::android::ScopedJavaLocalRef<jobject> j_callback =
      Java_CommentsServiceBridgeUnitTestCompanion_getBooleanCallback(
          env, j_companion());

  const base::Uuid kExpectedUuid = base::Uuid::GenerateRandomV4();

  EXPECT_CALL(service(), AddComment(_, _, _, _, _))
      .WillOnce(testing::WithArgs<4>(
          [&kExpectedUuid](base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(true);
            return kExpectedUuid;
          }));

  base::android::ScopedJavaLocalRef<jstring> j_returned_uuid =
      Java_CommentsServiceBridgeUnitTestCompanion_addComment(
          env, j_companion(), j_collab_id, j_gurl, j_content, j_parent_id,
          j_callback);

  EXPECT_EQ(kExpectedUuid,
            base::Uuid::ParseLowercase(
                base::android::ConvertJavaStringToUTF8(env, j_returned_uuid)));
  Java_CommentsServiceBridgeUnitTestCompanion_verifyBooleanCallback(
      env, j_companion(), true);
}

TEST_F(CommentsServiceBridgeTest, TestEditComment) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_comment_id =
      base::android::ConvertUTF8ToJavaString(
          env, base::Uuid::GenerateRandomV4().AsLowercaseString());
  base::android::ScopedJavaLocalRef<jstring> j_new_content =
      base::android::ConvertUTF8ToJavaString(env, "new content");
  base::android::ScopedJavaLocalRef<jobject> j_callback =
      Java_CommentsServiceBridgeUnitTestCompanion_getBooleanCallback(
          env, j_companion());
  EXPECT_CALL(service(), EditComment(_, _, _))
      .WillOnce(
          // Simulate success by running the C++ callback with 'true'.
          testing::WithArgs<2>([](base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(true);
          }));

  Java_CommentsServiceBridgeUnitTestCompanion_editComment(
      env, j_companion(), j_comment_id, j_new_content, j_callback);

  Java_CommentsServiceBridgeUnitTestCompanion_verifyBooleanCallback(
      env, j_companion(), true);
}

TEST_F(CommentsServiceBridgeTest, TestDeleteComment) {
  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jstring> j_comment_id =
      base::android::ConvertUTF8ToJavaString(
          env, base::Uuid::GenerateRandomV4().AsLowercaseString());
  base::android::ScopedJavaLocalRef<jobject> j_callback =
      Java_CommentsServiceBridgeUnitTestCompanion_getBooleanCallback(
          env, j_companion());

  EXPECT_CALL(service(), DeleteComment(_, _))
      .WillOnce(
          testing::WithArgs<1>([](base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          }));

  Java_CommentsServiceBridgeUnitTestCompanion_deleteComment(
      env, j_companion(), j_comment_id, j_callback);

  Java_CommentsServiceBridgeUnitTestCompanion_verifyBooleanCallback(
      env, j_companion(), false);
}

}  // namespace collaboration::comments::android

DEFINE_JNI(CommentsServiceBridgeUnitTestCompanion)
