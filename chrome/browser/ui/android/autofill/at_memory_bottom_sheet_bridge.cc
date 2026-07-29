// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/at_memory_bottom_sheet_bridge.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/to_vector.h"
#include "base/strings/string_util.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/personal_context/first_run/personal_context_first_run_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/at_memory_suggestion_controller.h"
#include "components/autofill/core/browser/ui/autofill_resource_utils.h"
#include "components/personal_context/first_run/personal_context_first_run_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/autofill/internal/jni_headers/AtMemoryBottomSheetBridge_jni.h"

namespace autofill {
namespace {

// Creates a Java `AutofillSuggestion` from a C++ `Suggestion`:
// - `main_text.value` (or `minor_texts[0].value` if `main_text` is empty) ->
// `label`
// - `labels[0]` (joined with spaces) -> `sublabel`
// - `icon` -> `iconId` (mapped via ResourceMapper)
// - `type` -> `suggestionType`
// - `children` -> `children`
// TODO(crbug.com/536821036): Add support for `payload` and pass name of the
// type there.
base::android::ScopedJavaLocalRef<jobject> CreateJavaSuggestion(
    JNIEnv* env,
    const Suggestion& suggestion) {
  std::u16string sub_label;
  if (!suggestion.labels.empty()) {
    sub_label = base::JoinString(
        base::ToVector(suggestion.labels[0], &Suggestion::Text::value), u" ");
  }

  std::u16string secondary_label;
  if (std::holds_alternative<Suggestion::AtMemoryPayload>(suggestion.payload)) {
    secondary_label =
        std::get<Suggestion::AtMemoryPayload>(suggestion.payload).type_name;
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

  // Some suggestions (e.g. `kAtMemorySourceAttribution`) store their display
  // text in `minor_texts` for Desktop Views styling. Fall back to `minor_texts`
  // so Java `AutofillSuggestion` gets a non-empty label.
  std::u16string label = suggestion.main_text.value;
  if (label.empty() && !suggestion.minor_texts.empty()) {
    label = suggestion.minor_texts[0].value;
  }

  return Java_AtMemoryBottomSheetBridge_createAutofillSuggestion(
      env, label, secondary_label, sub_label, android_icon_id,
      std::to_underlying(suggestion.type), children, suggestion.IsAcceptable(),
      suggestion.HasDeactivatedStyle());
}

}  // namespace

AtMemoryBottomSheetBridge::AtMemoryBottomSheetBridge(
    ui::WindowAndroid* window_android,
    Profile* profile,
    AtMemorySuggestionController* controller)
    : controller_(CHECK_DEREF(controller)) {
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

AtMemoryBottomSheetBridge::AtMemoryBottomSheetBridge(
    AtMemorySuggestionController* controller)
    : controller_(CHECK_DEREF(controller)) {}

AtMemoryBottomSheetBridge::~AtMemoryBottomSheetBridge() {
  if (java_object_) {
    Java_AtMemoryBottomSheetBridge_destroy(base::android::AttachCurrentThread(),
                                           java_object_);
  }
}

void AtMemoryBottomSheetBridge::RequestShowContent(
    base::span<const Suggestion> suggestions) {
  if (!java_object_) {
    controller_->OnDismissed();
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
  controller_->OnDismissed();
}

void AtMemoryBottomSheetBridge::OnQuerySubmitted(JNIEnv* env,
                                                 const std::u16string& query) {
  controller_->OnQuerySubmitted(query);
}

void AtMemoryBottomSheetBridge::OnQueryTextChanged(
    JNIEnv* env,
    const std::u16string& query) {
  controller_->OnQueryTextChanged(query);
}

void AtMemoryBottomSheetBridge::OnSuggestionDismissed(JNIEnv* env,
                                                      int position) {
  controller_->OnSuggestionDismissed(position);
}

void AtMemoryBottomSheetBridge::OnSuggestionSelected(JNIEnv* env,
                                                     int position) {
  controller_->OnSuggestionSelected(position);
}

void AtMemoryBottomSheetBridge::OnChildSuggestionsShown(JNIEnv* env,
                                                        int parent_position) {
  controller_->OnChildSuggestionsShown(parent_position);
}

void AtMemoryBottomSheetBridge::OnChildSuggestionSelected(JNIEnv* env,
                                                          int parent_position,
                                                          int child_position) {
  controller_->OnChildSuggestionSelected(parent_position, child_position);
}

bool AtMemoryBottomSheetBridge::IsSearching(JNIEnv* env) {
  return controller_->IsSearching();
}

}  // namespace autofill

DEFINE_JNI(AtMemoryBottomSheetBridge)
