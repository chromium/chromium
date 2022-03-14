// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/permission_prompt_android.h"

#include <vector>

#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/permissions/android/permission_dialog_delegate.h"
#include "components/permissions/permission_request.h"
#include "components/resources/android/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"

namespace permissions {

PermissionPromptAndroid::PermissionPromptAndroid(
    content::WebContents* web_contents,
    Delegate* delegate)
    : web_contents_(web_contents),
      delegate_(delegate),
      permission_infobar_(nullptr),
      weak_factory_(this) {
  DCHECK(web_contents);

  auto* permission_client = PermissionsClient::Get();
  if ((message_delegate_ = permission_client->MaybeCreateMessageUI(
           web_contents, GetContentSettingType(0u /* position */),
           weak_factory_.GetWeakPtr()))) {
    prompt_disposition_ = permissions::PermissionPromptDisposition::MESSAGE_UI;
  } else if ((permission_infobar_ = permission_client->MaybeCreateInfoBar(
                  web_contents, GetContentSettingType(0u /* position */),
                  weak_factory_.GetWeakPtr()))) {
    prompt_disposition_ =
        permissions::PermissionPromptDisposition::MINI_INFOBAR;
    permission_client->GetInfoBarManager(web_contents_)->AddObserver(this);
  } else {
    prompt_disposition_ =
        permissions::PermissionPromptDisposition::MODAL_DIALOG;
    PermissionDialogDelegate::Create(web_contents_, this);
  }
}

PermissionPromptAndroid::~PermissionPromptAndroid() {
  if (message_delegate_) {
    return;
  }
  infobars::InfoBarManager* infobar_manager =
      PermissionsClient::Get()->GetInfoBarManager(web_contents_);
  if (!infobar_manager)
    return;
  // RemoveObserver before RemoveInfoBar to not get notified about the removal
  // of the `permission_infobar_` infobar.
  infobar_manager->RemoveObserver(this);
  if (permission_infobar_) {
    infobar_manager->RemoveInfoBar(permission_infobar_);
  }
}

void PermissionPromptAndroid::UpdateAnchor() {
  NOTIMPLEMENTED();
}

permissions::PermissionPrompt::TabSwitchingBehavior
PermissionPromptAndroid::GetTabSwitchingBehavior() {
  return TabSwitchingBehavior::kKeepPromptAlive;
}

permissions::PermissionPromptDisposition
PermissionPromptAndroid::GetPromptDisposition() const {
  return prompt_disposition_;
}

void PermissionPromptAndroid::Closing() {
  delegate_->Dismiss();
}

void PermissionPromptAndroid::Accept() {
  delegate_->Accept();
}

void PermissionPromptAndroid::Deny() {
  delegate_->Deny();
}

void PermissionPromptAndroid::SetManageClicked() {
  delegate_->SetManageClicked();
}

void PermissionPromptAndroid::SetLearnMoreClicked() {
  delegate_->SetLearnMoreClicked();
}

bool PermissionPromptAndroid::ShouldCurrentRequestUseQuietUI() {
  return delegate_->ShouldCurrentRequestUseQuietUI();
}

absl::optional<PermissionUiSelector::QuietUiReason>
PermissionPromptAndroid::ReasonForUsingQuietUi() const {
  return delegate_->ReasonForUsingQuietUi();
}

size_t PermissionPromptAndroid::PermissionCount() const {
  return delegate_->Requests().size();
}

ContentSettingsType PermissionPromptAndroid::GetContentSettingType(
    size_t position) const {
  const std::vector<permissions::PermissionRequest*>& requests =
      delegate_->Requests();
  CHECK_LT(position, requests.size());
  return requests[position]->GetContentSettingsType();
}

static bool IsValidMediaRequestGroup(
    const std::vector<permissions::PermissionRequest*>& requests) {
  if (requests.size() < 2)
    return false;
  return (
      (requests[0]->request_type() == permissions::RequestType::kMicStream &&
       requests[1]->request_type() ==
           permissions::RequestType::kCameraStream) ||
      (requests[0]->request_type() == permissions::RequestType::kCameraStream &&
       requests[1]->request_type() == permissions::RequestType::kMicStream));
}

static bool IsValidARCameraAccessRequestGroup(
    const std::vector<permissions::PermissionRequest*>& requests) {
  if (requests.size() < 2)
    return false;
  return (
      (requests[0]->request_type() == permissions::RequestType::kArSession &&
       requests[1]->request_type() ==
           permissions::RequestType::kCameraStream) ||
      (requests[0]->request_type() == permissions::RequestType::kCameraStream &&
       requests[1]->request_type() == permissions::RequestType::kArSession));
}

// Grouped permission requests can only be Mic+Camera, Camera+Mic,
// AR + Camera, or Camera + AR.
static void CheckValidRequestGroup(
    const std::vector<permissions::PermissionRequest*>& requests) {
  DCHECK_EQ(static_cast<size_t>(2u), requests.size());
  DCHECK((IsValidMediaRequestGroup(requests)) ||
         (IsValidARCameraAccessRequestGroup(requests)));
}

int PermissionPromptAndroid::GetIconId() const {
  const std::vector<permissions::PermissionRequest*>& requests =
      delegate_->Requests();
  if (requests.size() == 1)
    return permissions::GetIconId(requests[0]->request_type());
  CheckValidRequestGroup(requests);
  return IDR_ANDROID_INFOBAR_MEDIA_STREAM_CAMERA;
}

std::u16string PermissionPromptAndroid::GetMessageText() const {
  const std::vector<permissions::PermissionRequest*>& requests =
      delegate_->Requests();
  if (requests.size() == 1) {
    if (requests[0]->request_type() ==
        permissions::RequestType::kStorageAccess) {
      return l10n_util::GetStringFUTF16(
          IDS_STORAGE_ACCESS_INFOBAR_TEXT,
          url_formatter::FormatUrlForSecurityDisplay(
              delegate_->GetRequestingOrigin(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC),
          url_formatter::FormatUrlForSecurityDisplay(
              delegate_->GetEmbeddingOrigin(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
    } else {
      return requests[0]->GetDialogMessageText();
    }
  }
  CheckValidRequestGroup(requests);
  if (IsValidARCameraAccessRequestGroup(requests)) {
    return l10n_util::GetStringFUTF16(
        IDS_AR_AND_MEDIA_CAPTURE_VIDEO_INFOBAR_TEXT,
        url_formatter::FormatUrlForSecurityDisplay(
            delegate_->GetRequestingOrigin(),
            url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_MEDIA_CAPTURE_AUDIO_AND_VIDEO_INFOBAR_TEXT,
        url_formatter::FormatUrlForSecurityDisplay(
            delegate_->GetRequestingOrigin(),
            url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
  }
}

void PermissionPromptAndroid::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                               bool animate) {
  if (infobar != permission_infobar_)
    return;

  permission_infobar_ = nullptr;
  infobars::InfoBarManager* infobar_manager =
      PermissionsClient::Get()->GetInfoBarManager(web_contents_);
  if (infobar_manager)
    infobar_manager->RemoveObserver(this);
}

void PermissionPromptAndroid::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  permission_infobar_ = nullptr;
  manager->RemoveObserver(this);
}

// static
std::unique_ptr<permissions::PermissionPrompt>
permissions::PermissionPrompt::Create(content::WebContents* web_contents,
                                      Delegate* delegate) {
  return std::make_unique<PermissionPromptAndroid>(web_contents, delegate);
}

}  // namespace permissions
