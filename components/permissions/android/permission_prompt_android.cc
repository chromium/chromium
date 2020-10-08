// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/permission_prompt_android.h"

#include <memory>

#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/permissions/android/permission_dialog_delegate.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permissions_client.h"
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

  infobars::InfoBarManager* infobar_manager =
      PermissionsClient::Get()->GetInfoBarManager(web_contents_);
  if (infobar_manager) {
    permission_infobar_ = PermissionsClient::Get()->MaybeCreateInfoBar(
        web_contents, GetContentSettingType(0u /* position */),
        weak_factory_.GetWeakPtr());
    if (permission_infobar_) {
      infobar_manager->AddObserver(this);
      return;
    }
  }

  PermissionDialogDelegate::Create(web_contents_, this);
}

PermissionPromptAndroid::~PermissionPromptAndroid() {
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

void PermissionPromptAndroid::UpdateAnchorPosition() {
  NOTREACHED() << "UpdateAnchorPosition is not implemented";
}

permissions::PermissionPrompt::TabSwitchingBehavior
PermissionPromptAndroid::GetTabSwitchingBehavior() {
  return TabSwitchingBehavior::kKeepPromptAlive;
}

permissions::PermissionPromptDisposition
PermissionPromptAndroid::GetPromptDisposition() const {
  return permission_infobar_
             ? permissions::PermissionPromptDisposition::MINI_INFOBAR
             : permissions::PermissionPromptDisposition::MODAL_DIALOG;
}

void PermissionPromptAndroid::Closing() {
  delegate_->Closing();
}

void PermissionPromptAndroid::Accept() {
  delegate_->Accept();
}

void PermissionPromptAndroid::Deny() {
  delegate_->Deny();
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
      (requests[0]->GetPermissionRequestType() ==
           permissions::PermissionRequestType::PERMISSION_MEDIASTREAM_MIC &&
       requests[1]->GetPermissionRequestType() ==
           permissions::PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA) ||
      (requests[0]->GetPermissionRequestType() ==
           permissions::PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA &&
       requests[1]->GetPermissionRequestType() ==
           permissions::PermissionRequestType::PERMISSION_MEDIASTREAM_MIC));
}

static bool IsValidARCameraAccessRequestGroup(
    const std::vector<permissions::PermissionRequest*>& requests) {
  if (requests.size() < 2)
    return false;
  return (
      (requests[0]->GetPermissionRequestType() ==
           permissions::PermissionRequestType::PERMISSION_AR &&
       requests[1]->GetPermissionRequestType() ==
           permissions::PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA) ||
      (requests[0]->GetPermissionRequestType() ==
           permissions::PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA &&
       requests[1]->GetPermissionRequestType() ==
           permissions::PermissionRequestType::PERMISSION_AR));
}

// Grouped permission requests can only be Mic+Camera, Camera+Mic,
// AR + Camera, or Camera + AR.
static void CheckValidRequestGroup(
    const std::vector<permissions::PermissionRequest*>& requests) {
  DCHECK_EQ(static_cast<size_t>(2u), requests.size());
  DCHECK_EQ(requests[0]->GetOrigin(), requests[1]->GetOrigin());
  DCHECK((IsValidMediaRequestGroup(requests)) ||
         (IsValidARCameraAccessRequestGroup(requests)));
}

int PermissionPromptAndroid::GetIconId() const {
  const std::vector<permissions::PermissionRequest*>& requests =
      delegate_->Requests();
  if (requests.size() == 1)
    return requests[0]->GetIconId();
  CheckValidRequestGroup(requests);
  return IDR_ANDROID_INFOBAR_MEDIA_STREAM_CAMERA;
}

base::string16 PermissionPromptAndroid::GetMessageText() const {
  const std::vector<permissions::PermissionRequest*>& requests =
      delegate_->Requests();
  if (requests.size() == 1) {
    if (requests[0]->GetContentSettingsType() ==
        ContentSettingsType::STORAGE_ACCESS) {
      return l10n_util::GetStringFUTF16(
          IDS_STORAGE_ACCESS_INFOBAR_TEXT,
          url_formatter::FormatUrlForSecurityDisplay(
              requests[0]->GetOrigin(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC),
          url_formatter::FormatUrlForSecurityDisplay(
              delegate_->GetEmbeddingOrigin(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
    } else {
      return requests[0]->GetMessageText();
    }
  }
  CheckValidRequestGroup(requests);
  if (IsValidARCameraAccessRequestGroup(requests)) {
    return l10n_util::GetStringFUTF16(
        IDS_AR_AND_MEDIA_CAPTURE_VIDEO_INFOBAR_TEXT,
        url_formatter::FormatUrlForSecurityDisplay(
            requests[0]->GetOrigin(),
            url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_MEDIA_CAPTURE_AUDIO_AND_VIDEO_INFOBAR_TEXT,
        url_formatter::FormatUrlForSecurityDisplay(
            requests[0]->GetOrigin(),
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
