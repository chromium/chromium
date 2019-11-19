// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TOOLBAR_LOCATION_BAR_MODEL_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_TOOLBAR_LOCATION_BAR_MODEL_ANDROID_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/ui/toolbar/chrome_location_bar_model_delegate.h"
#include "components/omnibox/browser/location_bar_model.h"

namespace content {
class WebContents;
}  // namespace content

// Owns a LocationBarModel and provides a way for Java to interact with it.
class LocationBarModelAndroid : public ChromeLocationBarModelDelegate {
 public:
  LocationBarModelAndroid(JNIEnv* env,
                          const base::android::JavaRef<jobject>& obj);
  ~LocationBarModelAndroid() override;

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jstring> GetFormattedFullURL(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jstring> GetURLForDisplay(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jstring> GetDisplaySearchTerms(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jint GetPageClassification(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             bool is_focused_from_fakebox);

  // ChromeLocationBarModelDelegate:
  content::WebContents* GetActiveWebContents() const override;
  bool IsInstantNTP() const override;

 private:
  std::unique_ptr<LocationBarModel> location_bar_model_;
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(LocationBarModelAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_TOOLBAR_LOCATION_BAR_MODEL_ANDROID_H_
