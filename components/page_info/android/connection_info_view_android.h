// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_ANDROID_CONNECTION_INFO_VIEW_ANDROID_H_
#define COMPONENTS_PAGE_INFO_ANDROID_CONNECTION_INFO_VIEW_ANDROID_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/page_info/page_info_ui.h"

namespace content {
class WebContents;
}

namespace page_info {
class PageInfoClient;
}

// Android implementation of the page info UI which displays detailed
// connection and certificate information for the website.
class ConnectionInfoViewAndroid : public PageInfoUI {
 public:
  ConnectionInfoViewAndroid(JNIEnv* env,
                            jobject java_page_info,
                            content::WebContents* web_contents);

  ConnectionInfoViewAndroid(const ConnectionInfoViewAndroid&) = delete;
  ConnectionInfoViewAndroid& operator=(const ConnectionInfoViewAndroid&) =
      delete;

  ~ConnectionInfoViewAndroid() override;
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Revokes any current user exceptions for bypassing SSL error interstitials
  // on this page.
  void ResetCertDecisions(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& java_web_contents);

  // PageInfoUI implementations.
  void SetIdentityInfo(const IdentityInfo& identity_info) override;

 private:
  // The presenter that controls the Page Info UI.
  std::unique_ptr<PageInfo> presenter_;

  // PageInfoClient outlives this class.
  raw_ptr<page_info::PageInfoClient> page_info_client_;

  // The java prompt implementation.
  base::android::ScopedJavaGlobalRef<jobject> popup_jobject_;
};

#endif  // COMPONENTS_PAGE_INFO_ANDROID_CONNECTION_INFO_VIEW_ANDROID_H_
