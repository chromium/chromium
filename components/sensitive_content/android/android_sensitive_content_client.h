// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SENSITIVE_CONTENT_ANDROID_ANDROID_SENSITIVE_CONTENT_CLIENT_H_
#define COMPONENTS_SENSITIVE_CONTENT_ANDROID_ANDROID_SENSITIVE_CONTENT_CLIENT_H_

#include "base/android/scoped_java_ref.h"
#include "components/sensitive_content/sensitive_content_client.h"
#include "components/sensitive_content/sensitive_content_manager.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace sensitive_content {

class AndroidSensitiveContentClient
    : public SensitiveContentClient,
      public content::WebContentsUserData<AndroidSensitiveContentClient> {
 public:
  AndroidSensitiveContentClient(content::WebContents* web_contents,
                                std::string histogram_prefix);

  AndroidSensitiveContentClient(const AndroidSensitiveContentClient&) = delete;
  AndroidSensitiveContentClient& operator=(
      const AndroidSensitiveContentClient&) = delete;
  ~AndroidSensitiveContentClient() override;

  // SensitiveContentClient:
  void SetContentSensitivity(bool content_is_sensitive) override;
  std::string_view GetHistogramPrefix() override;

 private:
  friend class content::WebContentsUserData<AndroidSensitiveContentClient>;

  SensitiveContentManager manager_;
  std::string histogram_prefix_;
  // Holds a reference to the Java counterpart of the client. Used to update the
  // content sensitivity on the Java view.
  base::android::ScopedJavaGlobalRef<jobject> java_sensitive_contents_client_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace sensitive_content

#endif  // COMPONENTS_SENSITIVE_CONTENT_ANDROID_ANDROID_SENSITIVE_CONTENT_CLIENT_H_
