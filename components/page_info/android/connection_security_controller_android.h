// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_ANDROID_CONNECTION_SECURITY_CONTROLLER_ANDROID_H_
#define COMPONENTS_PAGE_INFO_ANDROID_CONNECTION_SECURITY_CONTROLLER_ANDROID_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "components/page_info/page_info_ui.h"
#include "page_info_client.h"

namespace content {
class WebContents;
}

// ConnectionSecurityControllerAndroid is the C++ half of
// PageInfoConnectionSecurityController (in Java). It handles getting the
// identity info (reusing the same logic from desktop for which icons and
// strings to display instead of reimplementing that in Java) and it handles
// when the user revokes the decision to bypass SSL errors (which can only be
// handled in C++).
class ConnectionSecurityControllerAndroid : public PageInfoUI {
 public:
  ConnectionSecurityControllerAndroid(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& java_page_info,
      content::WebContents* web_contents);
  ConnectionSecurityControllerAndroid(
      const ConnectionSecurityControllerAndroid&) = delete;
  ConnectionSecurityControllerAndroid& operator=(
      const ConnectionSecurityControllerAndroid&) = delete;
  ~ConnectionSecurityControllerAndroid() override;

  void Destroy(JNIEnv* env);

  // PageInfoUI implementation
  void SetIdentityInfo(const IdentityInfo& identity_info) override;

  void LoadIdentityInfo(JNIEnv* env);

  void ResetCertDecisions(JNIEnv* env);

 private:
  std::unique_ptr<PageInfo> presenter_;
  raw_ptr<page_info::PageInfoClient> page_info_client_;
  base::android::ScopedJavaGlobalRef<jobject> controller_jobject_;
};

#endif  // COMPONENTS_PAGE_INFO_ANDROID_CONNECTION_SECURITY_CONTROLLER_ANDROID_H_
