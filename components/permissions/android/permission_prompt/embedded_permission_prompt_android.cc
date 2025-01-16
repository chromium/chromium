// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/permission_prompt/embedded_permission_prompt_android.h"

#include "components/permissions/android/permission_prompt/permission_dialog_delegate.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/resources/android/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace permissions {

using Variant = EmbeddedPermissionPromptFlowModel::Variant;

EmbeddedPermissionPromptAndroid::EmbeddedPermissionPromptAndroid(
    content::WebContents* web_contents,
    Delegate* delegate)
    : PermissionPromptAndroid(web_contents, delegate) {
  prompt_model_ = std::make_unique<EmbeddedPermissionPromptFlowModel>(
      web_contents, delegate);
  prompt_model_->CalculateCurrentVariant();
  CreatePermissionDialogDelegate();
}

EmbeddedPermissionPromptAndroid::~EmbeddedPermissionPromptAndroid() = default;

PermissionPromptDisposition
EmbeddedPermissionPromptAndroid::GetPromptDisposition() const {
  return PermissionPromptDisposition::MODAL_DIALOG;
}

bool EmbeddedPermissionPromptAndroid::ShouldFinalizeRequestAfterDecided()
    const {
  return false;
}

Variant EmbeddedPermissionPromptAndroid::GetEmbeddedPromptVariant() const {
  return prompt_model_->prompt_variant();
}

void EmbeddedPermissionPromptAndroid::Closing() {
  delegate()->Dismiss();
}

void EmbeddedPermissionPromptAndroid::Accept() {
  delegate()->Accept();
  MaybeUpdateDialogWithNewScreenVariant();
}

void EmbeddedPermissionPromptAndroid::AcceptThisTime() {
  delegate()->AcceptThisTime();
  MaybeUpdateDialogWithNewScreenVariant();
}

void EmbeddedPermissionPromptAndroid::Deny() {
  delegate()->Deny();
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
  // TODO(crbug.com/388407662): correct the title to send to Java.
  return PermissionRequest::GetDialogAnnotatedMessageText(
      url_formatter::FormatUrlForSecurityDisplay(
          delegate()->GetRequestingOrigin(),
          url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC),
      IDS_MEDIA_CAPTURE_AUDIO_AND_VIDEO_INFOBAR_TEXT,
      /*format_origin_bold=*/
      base::FeatureList::IsEnabled(permissions::features::kOneTimePermission));
}

bool EmbeddedPermissionPromptAndroid::ShouldUseRequestingOriginFavicon() const {
  return false;
}

void EmbeddedPermissionPromptAndroid::MaybeUpdateDialogWithNewScreenVariant() {
  prompt_model_->CalculateCurrentVariant();
  if (prompt_model_->prompt_variant() == Variant::kPreviouslyGranted) {
    delegate()->FinalizeCurrentRequests();
    return;
  }
  // TODO(crbug.com/388407640); update new screen.
}

}  // namespace permissions
