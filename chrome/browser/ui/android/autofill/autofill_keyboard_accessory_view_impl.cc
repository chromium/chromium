// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_keyboard_accessory_view_impl.h"

#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_controller.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/ui/autofill_resource_util.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/features/keyboard_accessory/internal/jni/AutofillKeyboardAccessoryViewBridge_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

namespace {

bool IsSuggestionTypeEligibleForKeyboardAccessory(SuggestionType type) {
  switch (type) {
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kUndo:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kAutofillAiPrivateInferenceNotice:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageAutofillAiIdentityDocs:
    case SuggestionType::kManageAutofillAiTravel:
    case SuggestionType::kManageAutofillAiShopping:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kManageEnhancedAutofill:
    case SuggestionType::kAutofillAiOtherOrders:
    case SuggestionType::kAutofillAiOtherShipments:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kLoadingThrobber:
    case SuggestionType::kBnplFootnote:
    case SuggestionType::kPersonalContextNotice:
    case SuggestionType::kAutocompleteAtMemoryButton:
    case SuggestionType::kAtMemoryOpenGemini:
    case SuggestionType::kAtMemorySearchResult:
    case SuggestionType::kAtMemoryInactivityNudge:
    case SuggestionType::kAtMemoryNoConnection:
    case SuggestionType::kAtMemorySearchAffordance:
    case SuggestionType::kAtMemoryGenericError:
    case SuggestionType::kAtMemoryAiDisclosure:
    case SuggestionType::kAtMemorySourceAttribution:
    case SuggestionType::kAtMemoryFetching:
    case SuggestionType::kAutofillAiSourceAttribution:
    case SuggestionType::kRemoveAutofillAi:
      return false;

    case SuggestionType::kAutocompleteEntry:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAddressEntry:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kLoyaltyCardEntry:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kAddressEntryOnTyping:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kBackupPasswordEntry:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kFillPassword:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kBnplEntry:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kIdentityCredential:
    case SuggestionType::kAllLoyaltyCardsEntry:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kWebauthnPasskeyQrCode:
    case SuggestionType::kOneTimePasswordEntry:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kFillAutofillAi:
    case SuggestionType::kPendingStateSignin:
    case SuggestionType::kFetchingAmbientData:
    case SuggestionType::kMaximizeCreditCardBenefitsEntry:
      return true;
  }
  NOTREACHED();
}

}  // namespace

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
  if (!view_android) {
    return false;
  }
  ui::WindowAndroid* window_android = view_android->GetWindowAndroid();
  if (!window_android) {
    return false;  // The window might not be attached (yet or anymore).
  }
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
    if (!IsSuggestionTypeEligibleForKeyboardAccessory(suggestion.type)) {
      continue;
    }
    int android_icon_id = 0;
    if (suggestion.icon != Suggestion::Icon::kNoIcon) {
      android_icon_id = ResourceMapper::MapToJavaDrawableId(
          GetIconResourceID(suggestion.icon));
    }

    std::u16string label = suggestion.main_text.value;
    std::u16string sublabel = base::JoinString(
        base::ToVector(suggestion.minor_texts, &Suggestion::Text::value), u" ");

    if (std::vector<std::vector<Suggestion::Text>> suggestion_labels =
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

    base::android::ScopedJavaLocalRef<jobject> payload;
    if (const Suggestion::AutofillProfilePayload* profile_payload =
            std::get_if<Suggestion::AutofillProfilePayload>(
                &suggestion.payload)) {
      payload = profile_payload->CreateJavaObject();
    } else if (const auto* ai_payload =
                   std::get_if<Suggestion::AutofillAiPayload>(
                       &suggestion.payload)) {
      payload = ai_payload->CreateJavaObject();
    } else if (const auto* at_memory_payload =
                   std::get_if<Suggestion::AtMemoryPayload>(
                       &suggestion.payload)) {
      payload = at_memory_payload->CreateJavaObject();
    }

    auto* custom_icon_url =
        std::get_if<Suggestion::CustomIconUrl>(&suggestion.custom_icon);

    java_suggestions.push_back(
        Java_AutofillKeyboardAccessoryViewBridge_createAutofillSuggestion(
            env, label, sublabel, suggestion.voice_over.value_or(u""),
            android_icon_id, std::to_underlying(suggestion.type),
            controller_->GetRemovalConfirmationText(i, nullptr),
            suggestion.iph_metadata.feature
                ? suggestion.iph_metadata.feature->name
                : "",
            suggestion.iph_description_text,
            custom_icon_url
                ? url::GURLAndroid::FromNativeGURL(env, **custom_icon_url)
                : url::GURLAndroid::EmptyGURL(env),
            std::to_underlying(suggestion.acceptability),
            *suggestion.is_loading, payload, i));
  }
  gfx::RectF bounds = controller_->element_bounds();
  Java_AutofillKeyboardAccessoryViewBridge_show(
      env, java_object_, std::move(java_suggestions),
      Java_AutofillKeyboardAccessoryViewBridge_createFieldBounds(
          env, bounds.x(), bounds.y(), bounds.right(), bounds.bottom()));
}

void AutofillKeyboardAccessoryViewImpl::ConfirmDeletion(
    const std::u16string& confirmation_title,
    const std::u16string& confirmation_body,
    const std::u16string& confirmation_body_link,
    const std::u16string& confirmation_button_text,
    base::OnceCallback<void(bool)> deletion_callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  deletion_callback_ = std::move(deletion_callback);
  Java_AutofillKeyboardAccessoryViewBridge_confirmDeletion(
      env, java_object_, confirmation_title, confirmation_body,
      confirmation_body_link, confirmation_button_text);
}

void AutofillKeyboardAccessoryViewImpl::SuggestionAccepted(JNIEnv* env,
                                                           int32_t list_index) {
  if (controller_) {
    controller_->AcceptSuggestion(
        list_index, AutofillMetrics::SuggestionAcceptedMethod::kTap);
  }
}

void AutofillKeyboardAccessoryViewImpl::SuggestionSelectionStateChanged(
    JNIEnv* env,
    int32_t list_index,
    bool is_selected) {
  if (!controller_) {
    return;
  }
  if (is_selected) {
    controller_->SelectSuggestion(list_index);
  } else {
    controller_->UnselectSuggestion();
  }
}

void AutofillKeyboardAccessoryViewImpl::DeletionRequested(JNIEnv* env,
                                                          int32_t list_index) {
  if (controller_) {
    controller_->RemoveSuggestion(
        list_index,
        AutofillMetrics::SingleEntryRemovalMethod::kKeyboardAccessory);
  }
}

void AutofillKeyboardAccessoryViewImpl::OnDeletionDialogClosed(JNIEnv* env,
                                                               bool confirmed) {
  if (deletion_callback_.is_null()) {
    LOG(DFATAL) << "OnDeletionDialogClosed called but no deletion is pending!";
    return;
  }
  std::move(deletion_callback_).Run(confirmed);
}

void AutofillKeyboardAccessoryViewImpl::ViewDismissed(JNIEnv* env) {
  if (controller_) {
    controller_->ViewDestroyed();
  }
}

void AutofillKeyboardAccessoryViewImpl::OpenSettingsForEntityType(
    JNIEnv* env,
    int32_t entity_type) {
  if (controller_) {
    controller_->OpenSettingsForEntityType(entity_type);
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

DEFINE_JNI(AutofillKeyboardAccessoryViewBridge)
