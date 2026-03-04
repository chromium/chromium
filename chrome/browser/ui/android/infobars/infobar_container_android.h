// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_INFOBAR_CONTAINER_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_INFOBAR_CONTAINER_ANDROID_H_

#include <stddef.h>

#include "base/android/scoped_java_ref.h"
#include "components/infobars/core/infobar_container.h"

class TabAndroid;

namespace content {
class WebContents;
}  // namespace content

class InfoBarContainerAndroid : public infobars::InfoBarContainer {
 public:
  explicit InfoBarContainerAndroid(TabAndroid* tab);

  InfoBarContainerAndroid(const InfoBarContainerAndroid&) = delete;
  InfoBarContainerAndroid& operator=(const InfoBarContainerAndroid&) = delete;

  void SetWebContents(JNIEnv* env, content::WebContents* web_contents);
  void Destroy(JNIEnv* env);

 private:
  ~InfoBarContainerAndroid() override;

  // InfobarContainer:
  void PlatformSpecificAddInfoBar(infobars::InfoBar* infobar,
                                  size_t position) override;
  void PlatformSpecificRemoveInfoBar(infobars::InfoBar* infobar) override;
  void PlatformSpecificReplaceInfoBar(infobars::InfoBar* old_infobar,
                                      infobars::InfoBar* new_infobar) override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject(JNIEnv* env);

  raw_ptr<TabAndroid> tab_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_INFOBAR_CONTAINER_ANDROID_H_
