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
#include "chrome/browser/personal_context/first_run/personal_context_first_run_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/ui/autofill_resource_utils.h"
#include "components/personal_context/first_run/personal_context_first_run_service.h"
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
// - `children` -> `children`
// TODO(crbug.com/502801668): Add support for `payload`.
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

  std::vector<base::android::ScopedJavaLocalRef<jobject>> children =
      base::ToVector(suggestion.children, [env](const Suggestion& child) {
        return CreateJavaSuggestion(env, child);
      });

  return Java_AtMemoryBottomSheetBridge_createAutofillSuggestion(
      env, suggestion.main_text.value, sub_label, android_icon_id,
      std::to_underlying(suggestion.type), children, suggestion.IsAcceptable());
}

}  // namespace

AtMemoryBottomSheetBridge::AtMemoryBottomSheetBridge(
    ui::WindowAndroid* window_android,
    Profile* profile) {
  CHECK(window_android);
  CHECK(profile);
  // AtMemoryBottomSheetBridge creates Java bottom sheet UI which depends on
  // `PersonalContextFirstRunService` to determine whether to show a notice
  // to the user.
  //
  // If AtMemory bottom sheet is shown, then `PersonalContextFirstRunService`
  // must exist for that profile.
  CHECK(PersonalContextFirstRunServiceFactory::GetForProfile(profile));

  java_object_ = Java_AtMemoryBottomSheetBridge_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      window_android->GetJavaObject(), profile);
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

void AtMemoryBottomSheetBridge::Hide() {
  if (java_object_) {
    Java_AtMemoryBottomSheetBridge_hide(base::android::AttachCurrentThread(),
                                        java_object_);
  }
}

void AtMemoryBottomSheetBridge::OnDismissed(JNIEnv* env) {
  if (delegate_) {
    delegate_->OnDismissed();
  }
  ResetDelegate();
}

void AtMemoryBottomSheetBridge::OnQuerySubmitted(JNIEnv* env,
                                                 const std::u16string& query) {
  if (delegate_) {
    delegate_->OnQuerySubmitted(query);
  }
}

void AtMemoryBottomSheetBridge::OnQueryTextChanged(
    JNIEnv* env,
    const std::u16string& query) {
  if (delegate_) {
    delegate_->OnQueryTextChanged(query);
  }
}

void AtMemoryBottomSheetBridge::OnSuggestionSelected(JNIEnv* env,
                                                     int position) {
  if (delegate_) {
    delegate_->OnSuggestionSelected(position);
  }
}

void AtMemoryBottomSheetBridge::OnChildSuggestionsShown(JNIEnv* env,
                                                        int parent_position) {
  if (delegate_) {
    delegate_->OnChildSuggestionsShown(parent_position);
  }
}

void AtMemoryBottomSheetBridge::OnChildSuggestionSelected(JNIEnv* env,
                                                          int parent_position,
                                                          int child_position) {
  if (delegate_) {
    delegate_->OnChildSuggestionSelected(parent_position, child_position);
  }
}

bool AtMemoryBottomSheetBridge::IsSearching(JNIEnv* env) {
  return delegate_ && delegate_->IsSearching();
}

void AtMemoryBottomSheetBridge::ResetDelegate() {
  delegate_.reset();
}

}  // namespace autofill

DEFINE_JNI(AtMemoryBottomSheetBridge)
