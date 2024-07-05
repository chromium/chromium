// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_keyboard_accessory_view_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/ui/android/autofill/autofill_accessibility_utils.h"
#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_controller.h"
#include "components/autofill/core/browser/ui/autofill_resource_utils.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/features/keyboard_accessory/internal/jni/AutofillKeyboardAccessoryViewBridge_jni.h"
#include "url/gurl.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

AutofillKeyboardAccessoryViewImpl::AutofillKeyboardAccessoryViewImpl(
    base::WeakPtr<AutofillKeyboardAccessoryController> controller)
    : controller_(controller) {
  java_object_.Reset(Java_AutofillKeyboardAccessoryViewBridge_create(
      base::android::AttachCurrentThread()));
}

AutofillKeyboardAccessoryViewImpl::~AutofillKeyboardAccessoryViewImpl() {
  Java_AutofillKeyboardAccessoryViewBridge_resetNativeViewPointer(
      base::android::AttachCurrentThread(), java_object_);
}

bool AutofillKeyboardAccessoryViewImpl::Initialize() {
  if (!controller_) {
    return false;
  }
  ui::ViewAndroid* view_android = controller_->container_view();
  if (!view_android)
    return false;
  ui::WindowAndroid* window_android = view_android->GetWindowAndroid();
  if (!window_android)
    return false;  // The window might not be attached (yet or anymore).
  Java_AutofillKeyboardAccessoryViewBridge_init(
      base::android::AttachCurrentThread(), java_object_,
      reinterpret_cast<intptr_t>(this), window_android->GetJavaObject());
  return true;
}

void AutofillKeyboardAccessoryViewImpl::Hide() {
  TRACE_EVENT0("passwords", "AutofillKeyboardAccessoryView::Hide");
  Java_AutofillKeyboardAccessoryViewBridge_dismiss(
      base::android::AttachCurrentThread(), java_object_);
}

void AutofillKeyboardAccessoryViewImpl::Show() {
  TRACE_EVENT0("passwords", "AutofillKeyboardAccessoryView::Show");
  if (!controller_) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  const int line_count = controller_->GetLineCount();
  std::vector<ScopedJavaLocalRef<jobject>> java_suggestions;
  java_suggestions.reserve(line_count);
  for (int i = 0; i < line_count; ++i) {
    const Suggestion& suggestion = controller_->GetSuggestionAt(i);
    int android_icon_id = 0;
    if (suggestion.icon != Suggestion::Icon::kNoIcon) {
      android_icon_id = ResourceMapper::MapToJavaDrawableId(
          GetIconResourceID(suggestion.icon));
    }

    std::u16string label = suggestion.main_text.value;
    std::u16string sublabel = suggestion.minor_text.value;
    if (std::vector<std::vector<autofill::Suggestion::Text>> suggestion_labels =
            controller_->GetSuggestionLabelsAt(i);
        !suggestion_labels.empty()) {
      // Verify that there is a single line of label, and it contains a single
      // item.
      DCHECK_EQ(suggestion_labels.size(), 1U);
      DCHECK_EQ(suggestion_labels[0].size(), 1U);

      // Since the keyboard accessory chips support showing only 2 strings, the
      // minor_text and the suggestion_labels are concatenated.
      if (sublabel.empty()) {
        sublabel = std::move(suggestion_labels[0][0].value);
      } else {
        sublabel = base::StrCat(
            {sublabel, u"  ", std::move(suggestion_labels[0][0].value)});
      }
    }

    auto* custom_icon_url =
        absl::get_if<Suggestion::CustomIconUrl>(&suggestion.custom_icon);
    java_suggestions.push_back(
        Java_AutofillKeyboardAccessoryViewBridge_createAutofillSuggestion(
            env, label, sublabel, android_icon_id,
            base::to_underlying(suggestion.type),
            controller_->GetRemovalConfirmationText(i, nullptr, nullptr),
            suggestion.feature_for_iph ? suggestion.feature_for_iph->name : "",
            suggestion.iph_description_text,
            custom_icon_url
                ? url::GURLAndroid::FromNativeGURL(env, **custom_icon_url)
                : url::GURLAndroid::EmptyGURL(env),
            suggestion.apply_deactivated_style));
  }
  Java_AutofillKeyboardAccessoryViewBridge_show(env, java_object_,
                                                std::move(java_suggestions));
}

void AutofillKeyboardAccessoryViewImpl::AxAnnounce(const std::u16string& text) {
  AnnounceTextForA11y(text);
}

void AutofillKeyboardAccessoryViewImpl::ConfirmDeletion(
    const std::u16string& confirmation_title,
    const std::u16string& confirmation_body,
    base::OnceCallback<void(bool)> deletion_callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  deletion_callback_ = std::move(deletion_callback);
  Java_AutofillKeyboardAccessoryViewBridge_confirmDeletion(
      env, java_object_, confirmation_title, confirmation_body);
}

void AutofillKeyboardAccessoryViewImpl::SuggestionSelected(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint list_index) {
  if (controller_) {
    controller_->AcceptSuggestion(list_index);
  }
}

void AutofillKeyboardAccessoryViewImpl::DeletionRequested(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint list_index) {
  if (controller_) {
    controller_->RemoveSuggestion(
        list_index,
        AutofillMetrics::SingleEntryRemovalMethod::kKeyboardAccessory);
  }
}

void AutofillKeyboardAccessoryViewImpl::OnDeletionDialogClosed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean confirmed) {
  if (deletion_callback_.is_null()) {
    LOG(DFATAL) << "OnDeletionDialogClosed called but no deletion is pending!";
    return;
  }
  std::move(deletion_callback_).Run(confirmed);
}

void AutofillKeyboardAccessoryViewImpl::ViewDismissed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (controller_) {
    controller_->ViewDestroyed();
  }
}

// static
std::unique_ptr<AutofillKeyboardAccessoryView>
AutofillKeyboardAccessoryView::Create(
    base::WeakPtr<AutofillKeyboardAccessoryController> controller) {
  auto view = std::make_unique<AutofillKeyboardAccessoryViewImpl>(controller);
  return view->Initialize() ? std::move(view) : nullptr;
}

}  // namespace autofill
