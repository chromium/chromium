// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/at_memory_bottom_sheet_bridge.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/containers/to_vector.h"
#include "base/strings/string_util.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/autofill/android/at_memory_bottom_sheet_delegate.h"
#include "components/autofill/core/browser/ui/autofill_resource_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/autofill/internal/jni_headers/AtMemoryBottomSheetBridge_jni.h"

namespace autofill {
namespace {

// Creates a Java `AutofillSuggestion` from a C++ `Suggestion`:
// - `main_text.value` -> `label`
// - `labels[0]` (joined with spaces) -> `sublabel`
// - `icon` -> `iconId` (mapped via ResourceMapper)
// - `type` -> `suggestionType`
// TODO(crbug.com/502801668): Add support for `payload` and `children`.
base::android::ScopedJavaLocalRef<jobject> CreateJavaSuggestion(
    JNIEnv* env,
    const Suggestion& suggestion) {
  std::u16string sub_label;
  if (!suggestion.labels.empty()) {
    sub_label = base::JoinString(
        base::ToVector(suggestion.labels[0], &Suggestion::Text::value), u" ");
  }

  int android_icon_id = 0;
  if (suggestion.icon != Suggestion::Icon::kNoIcon) {
    android_icon_id =
        ResourceMapper::MapToJavaDrawableId(GetIconResourceID(suggestion.icon));
  }

  return Java_AtMemoryBottomSheetBridge_createAutofillSuggestion(
      env,
      base::android::ConvertUTF16ToJavaString(env, suggestion.main_text.value),
      base::android::ConvertUTF16ToJavaString(env, sub_label), android_icon_id,
      std::to_underlying(suggestion.type));
}

}  // namespace

AtMemoryBottomSheetBridge::AtMemoryBottomSheetBridge(
    ui::WindowAndroid* window_android) {
  CHECK(window_android);
  java_object_ = Java_AtMemoryBottomSheetBridge_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      window_android->GetJavaObject());
}

AtMemoryBottomSheetBridge::~AtMemoryBottomSheetBridge() {
  if (java_object_) {
    Java_AtMemoryBottomSheetBridge_destroy(base::android::AttachCurrentThread(),
                                           java_object_);
  }
}

void AtMemoryBottomSheetBridge::RequestShowContent(
    std::unique_ptr<AtMemoryBottomSheetDelegate> delegate,
    base::span<const Suggestion> suggestions) {
  delegate_ = std::move(delegate);

  if (!java_object_) {
    if (delegate_) {
      delegate_->OnDismissed();
    }
    ResetDelegate();
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  std::vector<base::android::ScopedJavaLocalRef<jobject>> java_suggestions =
      base::ToVector(suggestions, [env](const Suggestion& suggestion) {
        return CreateJavaSuggestion(env, suggestion);
      });

  Java_AtMemoryBottomSheetBridge_show(env, java_object_,
                                      std::move(java_suggestions));
}

void AtMemoryBottomSheetBridge::OnDismissed(JNIEnv* env) {
  if (delegate_) {
    delegate_->OnDismissed();
  }
  ResetDelegate();
}

void AtMemoryBottomSheetBridge::ResetDelegate() {
  delegate_.reset();
}

}  // namespace autofill

DEFINE_JNI(AtMemoryBottomSheetBridge)
