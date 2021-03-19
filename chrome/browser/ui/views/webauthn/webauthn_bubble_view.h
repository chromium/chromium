// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_WEBAUTHN_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_WEBAUTHN_BUBBLE_VIEW_H_

#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"

#include "base/callback_forward.h"
#include "chrome/browser/ui/webauthn/account_hover_list_model.h"
#include "device/fido/public_key_credential_user_entity.h"

namespace content {
class WebContents;
}  // namespace content

// A bubble that appears near the omnibar when the user clicks a
// |WebAuthnIconView|, instructing the user that they can insert and tap their
// security key to log-in to the website.
//
// If there are available platform authenticator credentials, the user is
// presented with a list to choose from instead.
class WebAuthnBubbleView : public LocationBarBubbleDelegateView,
                           public AccountHoverListModel::Delegate {
 public:
  // Called when an account has been selected for the available platform
  // authenticator credentials case.
  using SelectedCallback = base::OnceCallback<void(size_t)>;

  // Creates a WebAuthnBubbleView owned by its widget.
  static WebAuthnBubbleView* Create(
      const std::string& relying_party_id,
      std::vector<device::PublicKeyCredentialUserEntity> users,
      SelectedCallback selected_callback,
      content::WebContents* web_contents);

  WebAuthnBubbleView(const std::string& relying_party_id,
                     std::vector<device::PublicKeyCredentialUserEntity> users,
                     base::OnceCallback<void(size_t)> selected_callback,
                     views::View* anchor_view,
                     content::WebContents* web_contents);
  ~WebAuthnBubbleView() override;
  WebAuthnBubbleView(const WebAuthnBubbleView& other) = delete;
  WebAuthnBubbleView& operator=(const WebAuthnBubbleView& other) = delete;

  // LocationBarBubbleDelegateView:
  std::u16string GetWindowTitle() const override;
  void Init() override;

 private:
  // AccountHoverListModel::Delegate:
  void OnItemSelected(int index) override;

  std::string relying_party_id_;
  std::vector<device::PublicKeyCredentialUserEntity> users_;
  SelectedCallback selected_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_WEBAUTHN_BUBBLE_VIEW_H_
