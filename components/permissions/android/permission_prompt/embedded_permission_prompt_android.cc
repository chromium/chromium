// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/permission_prompt/embedded_permission_prompt_android.h"

#include "base/android/jni_string.h"
#include "base/memory/weak_ptr.h"
#include "components/permissions/android/permission_prompt/permission_dialog_delegate.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permissions_client.h"
#include "components/resources/android/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace permissions {

using Variant = EmbeddedPermissionPromptFlowModel::Variant;
using Action = permissions::EmbeddedPermissionPromptFlowModel::DelegateAction;
using base::android::ConvertUTF16ToJavaString;

EmbeddedPermissionPromptAndroid::EmbeddedPermissionPromptAndroid(
    content::WebContents* web_contents,
    Delegate* delegate)
    : PermissionPromptAndroid(web_contents, delegate) {
  prompt_model_ = std::make_unique<EmbeddedPermissionPromptFlowModel>(
      web_contents, delegate);
  prompt_model_->CalculateCurrentVariant();
  CreatePermissionDialogDelegate();
  const auto& current_prompt_variant = prompt_model_->prompt_variant();
  // TODO(crbug.com/442793180): Plumb precise/approximate values when
  // <geolocation> prompts support it. Hardcoding to precise now to avoid double
  // prompting.
  if (current_prompt_variant == Variant::kAsk) {
    SetPromptOptions(GeolocationPromptOptions{GeolocationAccuracy::kPrecise});
  }
  prompt_model_->RecordElementAnchoredBubbleVariantUMA(current_prompt_variant);
  if (current_prompt_variant == Variant::kOsPrompt ||
      current_prompt_variant == Variant::kOsSystemSettings) {
    prompt_model_->StartFirstDisplayTime();
  }
}

EmbeddedPermissionPromptAndroid::~EmbeddedPermissionPromptAndroid() {
  if (!prompt_model_->HasDelegateActionSet()) {
    prompt_model_->SetDelegateAction(Action::kDismiss);
  }
}

// static
std::unique_ptr<EmbeddedPermissionPromptAndroid>
EmbeddedPermissionPromptAndroid::Create(content::WebContents* web_contents,
                                        Delegate* delegate) {
  auto prompt =
      std::make_unique<EmbeddedPermissionPromptAndroid>(web_contents, delegate);
  if (!prompt->permission_dialog_delegate() ||
      prompt->permission_dialog_delegate()->IsJavaDelegateDestroyed()) {
    return nullptr;
  }
  return prompt;
}

PermissionPromptDisposition
EmbeddedPermissionPromptAndroid::GetPromptDisposition() const {
  return PermissionPromptDisposition::ELEMENT_ANCHORED_BUBBLE;
}

bool EmbeddedPermissionPromptAndroid::ShouldFinalizeRequestAfterDecided()
    const {
  return false;
}

std::optional<gfx::Rect>
EmbeddedPermissionPromptAndroid::GetViewBoundsInScreen() const {
  // This is a modal prompt, the view bounds will cover the whole content
  // view.
  return web_contents()->GetContainerBounds();
}

bool EmbeddedPermissionPromptAndroid::IsAskPrompt() const {
  return (GetEmbeddedPromptVariant() == Variant::kAsk);
}

std::vector<permissions::ElementAnchoredBubbleVariant>
EmbeddedPermissionPromptAndroid::GetPromptVariants() const {
  std::vector<permissions::ElementAnchoredBubbleVariant> variants;
  return prompt_model_->GetPromptVariants();
}

Variant EmbeddedPermissionPromptAndroid::GetEmbeddedPromptVariant() const {
  return prompt_model_->prompt_variant();
}

void EmbeddedPermissionPromptAndroid::Closing() {
  prompt_model_->PrecalculateVariantsForMetrics();
  // TODO(crbug.com/388408021): in Android, there will be no x button and more
  // than only one dismiss reason of clicking outside the dialog. We are
  // grouping all of them into one single type for now and might expose others
  // later.
  prompt_model_->RecordOsMetrics(permissions::OsScreenAction::kDismissedScrim);
  prompt_model_->RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kDismissedScrim);
  prompt_model_->SetDelegateAction(Action::kDismiss);
  delegate()->FinalizeCurrentRequests();
}

void EmbeddedPermissionPromptAndroid::Accept() {
  prompt_model_->PrecalculateVariantsForMetrics();
  prompt_model_->RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kGranted);
  prompt_model_->SetDelegateAction(Action::kAllow);
  MaybeUpdateDialogWithNewScreenVariant();
}

void EmbeddedPermissionPromptAndroid::Acknowledge() {
  prompt_model_->RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kOk);
  prompt_model_->SetDelegateAction(Action::kDismiss);
  delegate()->FinalizeCurrentRequests();
}

void EmbeddedPermissionPromptAndroid::AcceptThisTime() {
  prompt_model_->PrecalculateVariantsForMetrics();
  prompt_model_->RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kGrantedOnce);
  prompt_model_->SetDelegateAction(Action::kAllowThisTime);
  MaybeUpdateDialogWithNewScreenVariant();
}

void EmbeddedPermissionPromptAndroid::Deny() {
  prompt_model_->PrecalculateVariantsForMetrics();
  prompt_model_->RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kDenied);
  prompt_model_->SetDelegateAction(Action::kDeny);
  delegate()->FinalizeCurrentRequests();
}

void EmbeddedPermissionPromptAndroid::Resumed() {
  MaybeUpdateDialogWithNewScreenVariant();
}

void EmbeddedPermissionPromptAndroid::SystemSettingsShown() {
  prompt_model_->RecordOsMetrics(permissions::OsScreenAction::kSystemSettings);
  prompt_model_->RecordPermissionActionUKM(
      permissions::ElementAnchoredBubbleAction::kSystemSettings);
}

void EmbeddedPermissionPromptAndroid::SystemPermissionResolved(bool accepted) {
  if (accepted) {
    prompt_model_->RecordOsMetrics(
        permissions::OsScreenAction::kOsPromptAllowed);
    MaybeUpdateDialogWithNewScreenVariant();
  } else {
    prompt_model_->PrecalculateVariantsForMetrics();
    prompt_model_->RecordOsMetrics(
        permissions::OsScreenAction::kOsPromptDenied);
    prompt_model_->SetDelegateAction(Action::kDismiss);
    delegate()->FinalizeCurrentRequests();
  }
}

bool EmbeddedPermissionPromptAndroid::ShouldCurrentRequestUseQuietUI() {
  return false;
}

std::optional<PermissionUiSelector::QuietUiReason>
EmbeddedPermissionPromptAndroid::ReasonForUsingQuietUi() const {
  return std::nullopt;
}

PermissionRequest::AnnotatedMessageText
EmbeddedPermissionPromptAndroid::GetAnnotatedMessageText() const {
  switch (GetEmbeddedPromptVariant()) {
    case Variant::kAsk: {
      const auto& requests = Requests();
      if (requests.size() == 1) {
        return requests[0]->GetDialogAnnotatedMessageText(
            delegate()->GetEmbeddingOrigin());
      }
      CheckValidRequestGroup(requests);
      return GetDialogAnnotatedMessageTextWithOrigin(
          IDS_MEDIA_CAPTURE_AUDIO_AND_VIDEO_INFOBAR_TEXT);
    }
    case Variant::kAdministratorGranted:
      return PermissionRequest::AnnotatedMessageText(
          l10n_util::GetStringFUTF16(IDS_EMBEDDED_PROMPT_ADMIN_ALLOWED,
                                     GetPermissionNameTextFragment()),
          /*bolded_ranges=*/{});
    case Variant::kPreviouslyGranted:
      return PermissionRequest::AnnotatedMessageText(
          l10n_util::GetStringFUTF16(IDS_EMBEDDED_PROMPT_PREVIOUSLY_ALLOWED,
                                     GetPermissionNameTextFragment()),
          /*bolded_ranges=*/{});
    case Variant::kOsSystemSettings:
      return PermissionRequest::AnnotatedMessageText(
          l10n_util::GetStringFUTF16(
              IDS_PERMISSION_OFF_FOR_CHROME, GetPermissionNameTextFragment(),
              PermissionsClient::Get()->GetClientApplicationName()),
          /*bolded_ranges=*/{});
    case Variant::kPreviouslyDenied:
      return PermissionRequest::AnnotatedMessageText(
          l10n_util::GetStringFUTF16(IDS_EMBEDDED_PROMPT_PREVIOUSLY_NOT_ALLOWED,
                                     GetPermissionNameTextFragment()),
          /*bolded_ranges=*/{});
    case Variant::kAdministratorDenied:
      return PermissionRequest::AnnotatedMessageText(
          l10n_util::GetStringFUTF16(IDS_EMBEDDED_PROMPT_ADMIN_BLOCKED,
                                     GetPermissionNameTextFragment()),
          /*bolded_ranges=*/{});
    case Variant::kOsPrompt:
      return PermissionRequest::AnnotatedMessageText(std::u16string(),
                                                     /*bolded_ranges=*/{});
    case Variant::kUninitialized:
      NOTREACHED();
  }
  NOTREACHED();
}

base::android::ScopedJavaLocalRef<jstring>
EmbeddedPermissionPromptAndroid::GetPositiveButtonText(JNIEnv* env,
                                                       bool is_one_time) const {
  switch (GetEmbeddedPromptVariant()) {
    case Variant::kAsk:
      return is_one_time
                 ? ConvertUTF16ToJavaString(
                       env, l10n_util::GetStringUTF16(
                                IDS_PERMISSION_ALLOW_WHILE_VISITING))
                 : ConvertUTF16ToJavaString(
                       env, l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW));

    case Variant::kPreviouslyGranted:
      return ConvertUTF16ToJavaString(
          env,
          l10n_util::GetStringUTF16(IDS_EMBEDDED_PROMPT_CONTINUE_ALLOWING));
    case Variant::kOsSystemSettings:
      return ConvertUTF16ToJavaString(
          env, l10n_util::GetStringFUTF16(
                   IDS_EMBEDDED_PROMPT_OPEN_SYSTEM_SETTINGS,
                   l10n_util::GetStringUTF16(IDS_ANDROID_NAME_FRAGMENT)));
    case Variant::kPreviouslyDenied:
      return ConvertUTF16ToJavaString(
          env,
          l10n_util::GetStringUTF16(IDS_EMBEDDED_PROMPT_CONTINUE_NOT_ALLOWING));
    case Variant::kAdministratorDenied:
    case Variant::kAdministratorGranted:
    case Variant::kOsPrompt:
      return ConvertUTF16ToJavaString(env, std::u16string_view());
    case Variant::kUninitialized:
      NOTREACHED();
  }
  NOTREACHED();
}

base::android::ScopedJavaLocalRef<jstring>
EmbeddedPermissionPromptAndroid::GetNegativeButtonText(JNIEnv* env,
                                                       bool is_one_time) const {
  switch (GetEmbeddedPromptVariant()) {
    case Variant::kAsk:
      return ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(IDS_PERMISSION_DONT_ALLOW));
    case Variant::kPreviouslyGranted:
      return ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(IDS_EMBEDDED_PROMPT_STOP_ALLOWING));
    case Variant::kOsSystemSettings:
      return ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(IDS_EMBEDDED_PROMPT_CANCEL_LABEL));
    case Variant::kPreviouslyDenied:
      return ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_THIS_TIME));
    case Variant::kAdministratorGranted:
    case Variant::kAdministratorDenied:
      return ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(IDS_EMBEDDED_PROMPT_OK_LABEL));
    case Variant::kOsPrompt:
      return ConvertUTF16ToJavaString(env, std::u16string_view());
    case Variant::kUninitialized:
      NOTREACHED();
  }
  NOTREACHED();
}
base::android::ScopedJavaLocalRef<jstring>
EmbeddedPermissionPromptAndroid::GetPositiveEphemeralButtonText(
    JNIEnv* env,
    bool is_one_time) const {
  if (!is_one_time || GetEmbeddedPromptVariant() !=
                          EmbeddedPermissionPromptFlowModel::Variant::kAsk) {
    return ConvertUTF16ToJavaString(env, std::u16string_view());
  }

  return ConvertUTF16ToJavaString(
      env, l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_THIS_TIME));
}

bool EmbeddedPermissionPromptAndroid::ShouldUseRequestingOriginFavicon() const {
  return false;
}

const std::vector<base::WeakPtr<permissions::PermissionRequest>>&
EmbeddedPermissionPromptAndroid::Requests() const {
  return prompt_model_->requests();
}

int EmbeddedPermissionPromptAndroid::GetIconId() const {
  if (prompt_model_->prompt_variant() == Variant::kAdministratorDenied ||
      prompt_model_->prompt_variant() == Variant::kAdministratorGranted) {
    return IDR_BUSINESS;
  }
  return PermissionPromptAndroid::GetIconId();
}

void EmbeddedPermissionPromptAndroid::MaybeUpdateDialogWithNewScreenVariant() {
  const auto& old_prompt_variant = prompt_model_->prompt_variant();
  prompt_model_->CalculateCurrentVariant();
  const auto& current_prompt_variant = prompt_model_->prompt_variant();
  if (current_prompt_variant == Variant::kPreviouslyGranted) {
    // Here the whole permission flow has already ended with permission allowed.
    // It's necessary to notify to Java side, for example to update omnibox
    // icon.
    permission_dialog_delegate()->NotifyPermissionAllowed();
    // TODO(crbug.com/374282626): change on renderer side, dispatching event not
    // simply following the action on the dialog but respecting how the
    // permission status change. Then we should translate the dismiss here to
    // "resolve" event if needed.
    prompt_model_->SetDelegateAction(Action::kDismiss);
    delegate()->FinalizeCurrentRequests();
    return;
  }
  if (current_prompt_variant != old_prompt_variant) {
    permission_dialog_delegate()->UpdateDialog();
    prompt_model_->RecordElementAnchoredBubbleVariantUMA(
        current_prompt_variant);
  }

  if (current_prompt_variant == Variant::kOsPrompt ||
      current_prompt_variant == Variant::kOsSystemSettings) {
    prompt_model_->StartFirstDisplayTime();
  }
}

PermissionRequest::AnnotatedMessageText
EmbeddedPermissionPromptAndroid::GetDialogAnnotatedMessageTextWithOrigin(
    int message_id) const {
  return PermissionRequest::GetDialogAnnotatedMessageText(
      url_formatter::FormatUrlForSecurityDisplay(
          delegate()->GetRequestingOrigin(),
          url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC),
      message_id,
      /*format_origin_bold=*/
      true);
}

std::u16string EmbeddedPermissionPromptAndroid::GetPermissionNameTextFragment()
    const {
  const auto& requests = Requests();
  std::u16string permission_name;
  if (requests.size() == 1) {
    return requests[0]->GetPermissionNameTextFragment();
  }
  CheckValidRequestGroup(requests);
  return l10n_util::GetStringUTF16(
      IDS_CAMERA_AND_MICROPHONE_PERMISSION_NAME_FRAGMENT);
}

}  // namespace permissions
