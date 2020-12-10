// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_FRAMEBUST_BLOCK_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_FRAMEBUST_BLOCK_INFOBAR_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/infobars/android/infobar_android.h"

namespace content {
class WebContents;
}

class FramebustBlockMessageDelegate;

// Communicates to the user about the intervention performed by the browser by
// blocking a framebust.
// That InfoBar shows a link to the URL that was blocked if the user wants to
// bypass the intervention, and a "OK" button to acknowledge and accept it.
// See FramebustBlockInfoBar.java for UI specifics.
class FramebustBlockInfoBar : public infobars::InfoBarAndroid {
 public:
  ~FramebustBlockInfoBar() override;

  static void Show(content::WebContents* web_contents,
                   std::unique_ptr<FramebustBlockMessageDelegate> delegate);

 private:
  explicit FramebustBlockInfoBar(
      std::unique_ptr<FramebustBlockMessageDelegate> delegate);

  // infobars::InfoBarAndroid:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;
  void ProcessButton(int action) override;
  void OnLinkClicked(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj) override;

  std::unique_ptr<FramebustBlockMessageDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(FramebustBlockInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_FRAMEBUST_BLOCK_INFOBAR_H_
