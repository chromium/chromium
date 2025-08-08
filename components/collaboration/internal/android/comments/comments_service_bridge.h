// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_ANDROID_COMMENTS_COMMENTS_SERVICE_BRIDGE_H_
#define COMPONENTS_COLLABORATION_INTERNAL_ANDROID_COMMENTS_COMMENTS_SERVICE_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "base/supports_user_data.h"
#include "components/collaboration/public/comments/comments_service.h"

namespace collaboration::comments::android {

class CommentsServiceBridge : public base::SupportsUserData::Data {
 public:
  static base::android::ScopedJavaLocalRef<jobject> GetBridgeForCommentsService(
      CommentsService* service);

  CommentsServiceBridge(const CommentsServiceBridge&) = delete;
  CommentsServiceBridge& operator=(const CommentsServiceBridge&) = delete;

  ~CommentsServiceBridge() override;

  static std::unique_ptr<CommentsServiceBridge> CreateForTest(
      CommentsService* service);

  // Methods called from Java via JNI.
  bool IsInitialized(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& j_caller);
  bool IsEmptyService(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& j_caller);
  base::android::ScopedJavaLocalRef<jstring> AddComment(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_caller,
      const base::android::JavaParamRef<jstring>& j_collaboration_id,
      const base::android::JavaParamRef<jobject>& j_url,
      const base::android::JavaParamRef<jstring>& j_content,
      const base::android::JavaParamRef<jstring>& j_parent_comment_id,
      const base::android::JavaParamRef<jobject>& j_success_callback);
  void EditComment(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_caller,
      const base::android::JavaParamRef<jstring>& j_comment_id,
      const base::android::JavaParamRef<jstring>& j_new_content,
      const base::android::JavaParamRef<jobject>& j_success_callback);
  void DeleteComment(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_caller,
      const base::android::JavaParamRef<jstring>& j_comment_id,
      const base::android::JavaParamRef<jobject>& j_success_callback);
  void QueryComments(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_caller,
      const base::android::JavaParamRef<jobject>& j_filter_criteria,
      const base::android::JavaParamRef<jobject>& j_pagination_criteria,
      const base::android::JavaParamRef<jobject>& j_callback);
  void AddObserver(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_caller,
      const base::android::JavaParamRef<jobject>& j_observer,
      const base::android::JavaParamRef<jobject>& j_filter_criteria);
  void RemoveObserver(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& j_caller,
                      const base::android::JavaParamRef<jobject>& j_observer);

  // Returns the companion Java object for this bridge.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  friend class CommentsServiceBridgeTest;
  explicit CommentsServiceBridge(CommentsService* service);

  raw_ptr<CommentsService> service_;

  // A reference to the Java counterpart of this class.  See
  // CommentsServiceBridge.java.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace collaboration::comments::android

#endif  // COMPONENTS_COLLABORATION_INTERNAL_ANDROID_COMMENTS_COMMENTS_SERVICE_BRIDGE_H_
