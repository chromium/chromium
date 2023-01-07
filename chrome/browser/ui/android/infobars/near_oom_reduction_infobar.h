// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_NEAR_OOM_REDUCTION_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_NEAR_OOM_REDUCTION_INFOBAR_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/infobars/android/infobar_android.h"

namespace content {
class WebContents;
}

class InterventionDelegate;

// Communicates to the user about the intervention performed by the browser to
// limit the page's memory usage. See NearOomReductionInfoBar.java for UI
// specifics, and NearOomMessageDelegate for behavior specifics.
class NearOomReductionInfoBar : public infobars::InfoBarAndroid {
 public:
  NearOomReductionInfoBar(const NearOomReductionInfoBar&) = delete;
  NearOomReductionInfoBar& operator=(const NearOomReductionInfoBar&) = delete;

  ~NearOomReductionInfoBar() override;

  static void Show(content::WebContents* web_contents,
                   InterventionDelegate* delegate);

 private:
  explicit NearOomReductionInfoBar(InterventionDelegate* delegate);

  // infobars::InfoBarAndroid:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;
  void OnLinkClicked(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj) override;
  void ProcessButton(int action) override;

  raw_ptr<InterventionDelegate> delegate_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_NEAR_OOM_REDUCTION_INFOBAR_H_
