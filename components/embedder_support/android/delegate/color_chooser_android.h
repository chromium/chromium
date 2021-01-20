// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_DELEGATE_COLOR_CHOOSER_ANDROID_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_DELEGATE_COLOR_CHOOSER_ANDROID_H_

#include <vector>

#include "base/android/jni_android.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/public/browser/color_chooser.h"
#include "third_party/blink/public/mojom/choosers/color_chooser.mojom.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace content {
class WebContents;
}

namespace web_contents_delegate_android {

// Glues the Java (ColorPickerChooser.java) picker with the native part.
class ColorChooserAndroid : public content::ColorChooser {
 public:
  ColorChooserAndroid(
      content::WebContents* tab,
      SkColor initial_color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions);
  ~ColorChooserAndroid() override;

  void OnColorChosen(JNIEnv* env,
                     const base::android::JavaRef<jobject>& obj,
                     jint color);

  // ColorChooser interface
  void End() override;
  void SetSelectedColor(SkColor color) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_color_chooser_;

  // The web contents invoking the color chooser.  No ownership. because it will
  // outlive this class.
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(ColorChooserAndroid);
};

}  // namespace web_contents_delegate_android

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ANDROID_DELEGATE_COLOR_CHOOSER_ANDROID_H_
