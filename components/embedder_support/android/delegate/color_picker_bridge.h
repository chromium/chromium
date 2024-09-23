// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_DELEGATE_COLOR_PICKER_BRIDGE_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_DELEGATE_COLOR_PICKER_BRIDGE_H_

#include <vector>

#include "base/android/jni_android.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/color_chooser.h"
#include "third_party/blink/public/mojom/choosers/color_chooser.mojom.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace content {
class WebContents;
}

namespace web_contents_delegate_android {

// Glues the Java (ColorPickerDialogView.java) picker with the native part.
class ColorPickerBridge : public content::ColorChooser {
 public:
  ColorPickerBridge(
      content::WebContents* tab,
      SkColor initial_color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions);

  ColorPickerBridge(const ColorPickerBridge&) = delete;
  ColorPickerBridge& operator=(const ColorPickerBridge&) = delete;

  ~ColorPickerBridge() override;

  void OnColorChosen(JNIEnv* env,
                     const base::android::JavaRef<jobject>& obj,
                     jint color);

  // ColorPicker interface
  void End() override;
  void SetSelectedColor(SkColor color) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_color_chooser_;

  // The web contents invoking the color chooser.  No ownership. because it will
  // outlive this class.
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace web_contents_delegate_android

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ANDROID_DELEGATE_COLOR_PICKER_BRIDGE_H_
