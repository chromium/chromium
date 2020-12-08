// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_WEBID_PERMISSION_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_WEBID_PERMISSION_INFOBAR_H_

#include "base/callback.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "content/public/browser/identity_request_dialog_controller.h"

// Basic infobar that is used to ask for user approval in various WebID flows.
//
// Currently it is  used to obtain user approval on initial information exchange
// between the Relying Party and Identity Provider to exchange informations.
class WebIDPermissionInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  using UserApproval = content::IdentityRequestDialogController::UserApproval;
  using Callback =
      content::IdentityRequestDialogController::InitialApprovalCallback;

  WebIDPermissionInfoBarDelegate(const base::string16& message,
                                 Callback callback);
  ~WebIDPermissionInfoBarDelegate() override;

 private:
  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  const base::string16 message_;
  Callback callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_WEBID_PERMISSION_INFOBAR_H_
