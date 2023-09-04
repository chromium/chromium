// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_popup_view_android.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/android/chrome_jni_headers/AutofillPopupBridge_jni.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/ui/android/autofill/autofill_accessibility_utils.h"
#include "chrome/browser/ui/android/autofill/autofill_keyboard_accessory_view.h"
#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_adapter.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "components/autofill/core/browser/ui/autofill_resource_utils.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/security_state/core/security_state.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/android/gurl_android.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

AutofillPopupViewAndroid::AutofillPopupViewAndroid(
    base::WeakPtr<AutofillPopupController> controller)
    : controller_(controller), deleting_index_(-1) {}

AutofillPopupViewAndroid::~AutofillPopupViewAndroid() {}

bool AutofillPopupViewAndroid::Show(
    AutoselectFirstSuggestion autoselect_first_suggestion) {
  OnSuggestionsChanged();
  return true;
}

void AutofillPopupViewAndroid::Hide() {
  controller_ = nullptr;
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!java_object_.is_null()) {
    Java_AutofillPopupBridge_dismiss(env, java_object_);
  } else {
    // Hide() should delete |this| either via Java dismiss or directly.
    delete this;
  }
}

bool AutofillPopupViewAndroid::OverlapsWithPictureInPictureWindow() const {
  return false;
}

bool AutofillPopupViewAndroid::HandleKeyPressEvent(
    const content::NativeWebKeyboardEvent& event) {
  return false;
}

void AutofillPopupViewAndroid::OnSuggestionsChanged() {
  if (java_object_.is_null()) {
    return;
  }

  const ScopedJavaLocalRef<jobject> view = popup_view_.view();
  if (view.is_null()) {
    return;
  }

  ui::ViewAndroid* view_android = controller_->container_view();

  DCHECK(view_android);
  JNIEnv* env = base::android::AttachCurrentThread();
  view_android->SetAnchorRect(view, controller_->element_bounds());

  size_t count = controller_->GetLineCount();
  ScopedJavaLocalRef<jobjectArray> data_array =
      Java_AutofillPopupBridge_createAutofillSuggestionArray(env, count);

  for (size_t i = 0; i < count; ++i) {
    std::u16string label;
    std::u16string secondary_label;
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableVirtualCardMetadata) &&
        base::FeatureList::IsEnabled(
            features::kAutofillEnableCardProductName)) {
      label = controller_->GetSuggestionMainTextAt(i);
      secondary_label = controller_->GetSuggestionMinorTextAt(i);
    } else {
      label = controller_->GetSuggestionMinorTextAt(i).empty()
                  ? controller_->GetSuggestionMainTextAt(i)
                  : base::StrCat({controller_->GetSuggestionMainTextAt(i), u" ",
                                  controller_->GetSuggestionMinorTextAt(i)});
    }
    std::vector<std::vector<autofill::Suggestion::Text>> suggestion_labels =
        controller_->GetSuggestionLabelsAt(i);
    std::u16string sublabel;
    std::u16string secondary_sublabel;
    std::u16string item_tag;
    DCHECK_LE(suggestion_labels.size(), 2U);
    if (suggestion_labels.size() > 0) {
      DCHECK_LE(suggestion_labels[0].size(), 2U);
      sublabel = std::move(suggestion_labels[0][0].value);
      if (suggestion_labels[0].size() > 1) {
        secondary_sublabel = std::move(suggestion_labels[0][1].value);
      }
    }
    if (suggestion_labels.size() > 1) {
      DCHECK_EQ(suggestion_labels[1].size(), 1U);
      item_tag = std::move(suggestion_labels[1][0].value);
    }

    int android_icon_id = 0;

    const Suggestion& suggestion = controller_->GetSuggestionAt(i);
    if (!suggestion.icon.empty()) {
      android_icon_id = ResourceMapper::MapToJavaDrawableId(
          GetIconResourceID(suggestion.icon));
    }

    bool is_deletable =
        controller_->GetRemovalConfirmationText(i, nullptr, nullptr);
    bool is_label_multiline =
        suggestion.popup_item_id ==
            PopupItemId::kInsecureContextPaymentDisabledMessage ||
        suggestion.popup_item_id == PopupItemId::kMixedFormMessage;

    Java_AutofillPopupBridge_addToAutofillSuggestionArray(
        env, java_object_, data_array, i,
        base::android::ConvertUTF16ToJavaString(env, label),
        base::android::ConvertUTF16ToJavaString(env, secondary_label),
        base::android::ConvertUTF16ToJavaString(env, sublabel),
        base::android::ConvertUTF16ToJavaString(env, secondary_sublabel),
        base::android::ConvertUTF16ToJavaString(env, item_tag), android_icon_id,
        suggestion.is_icon_at_start,
        base::to_underlying(suggestion.popup_item_id), is_deletable,
        is_label_multiline, /*isLabelBold*/ false,
        url::GURLAndroid::FromNativeGURL(env, suggestion.custom_icon_url));
  }

  Java_AutofillPopupBridge_show(env, java_object_, data_array,
                                base::i18n::IsRTL());
}

void AutofillPopupViewAndroid::AxAnnounce(const std::u16string& text) {
  AnnounceTextForA11y(text);
}

absl::optional<int32_t> AutofillPopupViewAndroid::GetAxUniqueId() {
  NOTIMPLEMENTED() << "See https://crbug.com/985927";
  return absl::nullopt;
}

base::WeakPtr<AutofillPopupView> AutofillPopupViewAndroid::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<AutofillPopupView> AutofillPopupViewAndroid::CreateSubPopupView(
    base::WeakPtr<AutofillPopupController> controller) {
  NOTIMPLEMENTED() << "No sub-popups on Android";
  return nullptr;
}

void AutofillPopupViewAndroid::SuggestionSelected(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint list_index) {
  // Race: Hide() may have already run.
  if (!controller_) {
    return;
  }

  controller_->AcceptSuggestion(list_index, base::TimeTicks::Now());
}

void AutofillPopupViewAndroid::DeletionRequested(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint list_index) {
  if (!controller_ || java_object_.is_null()) {
    return;
  }

  std::u16string confirmation_title, confirmation_body;
  if (!controller_->GetRemovalConfirmationText(list_index, &confirmation_title,
                                               &confirmation_body)) {
    return;
  }

  deleting_index_ = list_index;
  Java_AutofillPopupBridge_confirmDeletion(
      env, java_object_,
      base::android::ConvertUTF16ToJavaString(env, confirmation_title),
      base::android::ConvertUTF16ToJavaString(env, confirmation_body));
}

void AutofillPopupViewAndroid::DeletionConfirmed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!controller_) {
    return;
  }

  CHECK_GE(deleting_index_, 0);
  controller_->RemoveSuggestion(deleting_index_);
}

void AutofillPopupViewAndroid::PopupDismissed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (controller_) {
    controller_->ViewDestroyed();
  }

  // The controller has now deleted itself. Remove dangling weak reference.
  controller_ = nullptr;
  delete this;
}

bool AutofillPopupViewAndroid::Init() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = controller_->container_view();

  DCHECK(view_android);
  popup_view_ = view_android->AcquireAnchorView();
  const ScopedJavaLocalRef<jobject> view = popup_view_.view();
  if (view.is_null()) {
    return false;
  }
  ui::WindowAndroid* window_android = view_android->GetWindowAndroid();
  if (!window_android) {
    return false;  // The window might not be attached (yet or anymore).
  }

  java_object_.Reset(Java_AutofillPopupBridge_create(
      env, view, reinterpret_cast<intptr_t>(this),
      window_android->GetJavaObject()));
  return true;
}

bool AutofillPopupViewAndroid::WasSuppressed() {
  return java_object_ &&
         Java_AutofillPopupBridge_wasSuppressed(
             base::android::AttachCurrentThread(), java_object_);
}

// static
base::WeakPtr<AutofillPopupView> AutofillPopupView::Create(
    base::WeakPtr<AutofillPopupController> controller) {
  auto adapter = std::make_unique<AutofillKeyboardAccessoryAdapter>(controller);
  auto accessory_view = std::make_unique<AutofillKeyboardAccessoryView>(
      adapter->GetWeakPtrToAdapter());
  if (!accessory_view->Initialize()) {
    return nullptr;  // Don't create an adapter without initialized view.
  }

  adapter->SetAccessoryView(std::move(accessory_view));
  return adapter.release()->GetWeakPtr();
}

}  // namespace autofill
