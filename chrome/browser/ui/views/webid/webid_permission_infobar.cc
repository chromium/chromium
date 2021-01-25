// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/webid_permission_infobar.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/webid/identity_dialogs.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

void ShowWebIdPermissionInfoBar(
    content::WebContents* web_contents,
    const base::string16& message,
    WebIdPermissionInfoBarDelegate::Callback callback) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);

  auto delegate = std::make_unique<WebIdPermissionInfoBarDelegate>(
      message, std::move(callback));
  infobar_service->AddInfoBar(
      infobar_service->CreateConfirmInfoBar(std::move(delegate)));
}

WebIdPermissionInfoBarDelegate::WebIdPermissionInfoBarDelegate(
    const base::string16& message,
    WebIdPermissionInfoBarDelegate::Callback callback)
    : message_(message), callback_(std::move(callback)) {
  DCHECK(callback_);
}

WebIdPermissionInfoBarDelegate::~WebIdPermissionInfoBarDelegate() {
  if (callback_) {
    // The infobar has closed without the user expressing an explicit
    // preference. The current request should be denied.
    std::move(callback_).Run(UserApproval::kDenied);
  }
}

infobars::InfoBarDelegate::InfoBarIdentifier
WebIdPermissionInfoBarDelegate::GetIdentifier() const {
  return WEBID_PERMISSION_INFOBAR_DELEGATE;
}

base::string16 WebIdPermissionInfoBarDelegate::GetMessageText() const {
  return message_;
}

base::string16 WebIdPermissionInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ? IDS_PERMISSION_ALLOW
                                                         : IDS_PERMISSION_DENY);
}

bool WebIdPermissionInfoBarDelegate::Accept() {
  std::move(callback_).Run(UserApproval::kApproved);
  return true;
}

bool WebIdPermissionInfoBarDelegate::Cancel() {
  std::move(callback_).Run(UserApproval::kDenied);
  return true;
}
