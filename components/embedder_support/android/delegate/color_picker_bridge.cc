// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/delegate/color_picker_bridge.h"

#include <stddef.h>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/embedder_support/android/web_contents_delegate_jni_headers/ColorPickerBridge_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;

namespace web_contents_delegate_android {

ColorPickerBridge::ColorPickerBridge(
    content::WebContents* web_contents,
    SkColor initial_color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions)
    : web_contents_(web_contents) {
  JNIEnv* env = AttachCurrentThread();

  // End with the initial color if no window was found.
  auto* window_android = web_contents->GetNativeView()->GetWindowAndroid();
  if (!window_android) {
    OnColorChosen(env, j_color_chooser_, initial_color);
    return;
  }

  // Create a bridge to communicate with Java-side code.
  j_color_chooser_.Reset(Java_ColorPickerBridge_create(
      env, reinterpret_cast<intptr_t>(this), window_android->GetJavaObject()));

  // End with the initial color if the bridge creation failed.
  if (j_color_chooser_.is_null()) {
    OnColorChosen(env, j_color_chooser_, initial_color);
    return;
  }

  // For an element that includes suggestions, send them to Java.
  for (const auto& suggestion : suggestions) {
    Java_ColorPickerBridge_addColorSuggestion(
        env, j_color_chooser_, suggestion->color,
        ConvertUTF8ToJavaString(env, suggestion->label));
  }

  // Show the color picker dialog.
  Java_ColorPickerBridge_showColorPicker(env, j_color_chooser_, initial_color);
}

ColorPickerBridge::~ColorPickerBridge() = default;

void ColorPickerBridge::End() {
  if (!j_color_chooser_.is_null()) {
    JNIEnv* env = AttachCurrentThread();
    Java_ColorPickerBridge_closeColorPicker(env, j_color_chooser_);
  }
}

void ColorPickerBridge::SetSelectedColor(SkColor color) {
  // Not implemented since the color is set on the java side only, in theory
  // it can be set from JS which would override the user selection but
  // we don't support that for now.
}

void ColorPickerBridge::OnColorChosen(JNIEnv* env,
                                      const JavaRef<jobject>& obj,
                                      jint color) {
  web_contents_->DidChooseColorInColorChooser(color);
  web_contents_->DidEndColorChooser();
}

}  // namespace web_contents_delegate_android
