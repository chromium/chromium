// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TOOLBAR_TOOLBAR_MODEL_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_TOOLBAR_TOOLBAR_MODEL_ANDROID_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/ui/toolbar/chrome_toolbar_model_delegate.h"
#include "components/omnibox/browser/toolbar_model.h"

namespace content {
class WebContents;
}  // content

// Owns a ToolbarModel and provides a way for Java to interact with it.
class ToolbarModelAndroid : public ChromeToolbarModelDelegate {
 public:
  ToolbarModelAndroid(JNIEnv* env, const base::android::JavaRef<jobject>& obj);
  ~ToolbarModelAndroid() override;

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jstring> GetFormattedFullURL(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jstring> GetURLForDisplay(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // ChromeToolbarModelDelegate:
  content::WebContents* GetActiveWebContents() const override;

 private:
  std::unique_ptr<ToolbarModel> toolbar_model_;
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ToolbarModelAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_TOOLBAR_TOOLBAR_MODEL_ANDROID_H_
