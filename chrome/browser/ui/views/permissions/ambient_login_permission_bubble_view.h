// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_AMBIENT_LOGIN_PERMISSION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_AMBIENT_LOGIN_PERMISSION_BUBBLE_VIEW_H_

#include <cstddef>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

class HoverButton;

namespace ambient_signin {
class AmbientLoginPermissionRequest;
struct FederatedCredential;
struct PasskeyOrPasswordCredential;
}  // namespace ambient_signin

namespace content {
class WebContents;
}

namespace ui {
class Event;
}

namespace views {
class MdTextButton;
}

// Bubble view for ambient sign-in requests (passkeys, passwords, and federated
// credentials) managed through PermissionRequestManager.
//
// This is used for JS calls to `navigator.credentials.get()` where there are
// optional sign-in situations; the user can select a credential with which to
// sign in, or else dismiss or ignore the bubble. In FedCM this corresponds to
//  "passive mode" requests, and in WebAuthn it means conditional UI requests
// that do not attach use autofill, i.e.
// `mediation:'conditional', uiMode:'passive'`.
//
// Initially displays a compact summary for the first credential with a "Sign
// in" button and an expand button:
// |-------------------------------------------------------|
// | Title                                             [X] |
// |-------------------------------------------------------|
// | [Icon]  Username                      [ Sign in ] [v] |
// |         Provider (optional)                           |
// |-------------------------------------------------------|
//
// Clicking the expand button [v] replaces the summary with a scrollable list of
// all available credentials, where clicking any row selects and accepts it:
// |-------------------------------------------------------|
// | Title                                             [X] |
// |-------------------------------------------------------|
// | [Icon]  Display name / Username                       |
// |         Username (optional)                           |
// |         Provider                                      |
// | [Icon]  Display name / Username                       |
// |         Provider                                      |
// | ...                                                   |
// |-------------------------------------------------------|
class AmbientLoginPermissionBubbleView : public PermissionPromptBubbleBaseView {
  METADATA_HEADER(AmbientLoginPermissionBubbleView,
                  PermissionPromptBubbleBaseView)

 public:
  AmbientLoginPermissionBubbleView(
      content::WebContents* web_contents,
      base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
      PermissionPromptStyle prompt_style);
  AmbientLoginPermissionBubbleView(const AmbientLoginPermissionBubbleView&) =
      delete;
  AmbientLoginPermissionBubbleView& operator=(
      const AmbientLoginPermissionBubbleView&) = delete;
  ~AmbientLoginPermissionBubbleView() override;

  // PermissionPromptBubbleBaseView:
  void Show() override;

  // views::View:
  void AddedToWidget() override;

 private:
  ambient_signin::AmbientLoginPermissionRequest* GetAmbientRequest();
  void OnSignInButtonClicked(const ui::Event& event);
  void OnExpandButtonClicked(const ui::Event& event);
  void OnCredentialClicked(size_t index, const ui::Event& event);
  void OnFederatedCredentialClicked(size_t index, const ui::Event& event);
  std::unique_ptr<HoverButton> CreateCredentialRow(
      const ambient_signin::PasskeyOrPasswordCredential& credential,
      size_t index);
  std::unique_ptr<HoverButton> CreateFederatedCredentialRow(
      const ambient_signin::FederatedCredential& credential,
      size_t index);

  raw_ptr<views::MdTextButton> sign_in_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_AMBIENT_LOGIN_PERMISSION_BUBBLE_VIEW_H_
