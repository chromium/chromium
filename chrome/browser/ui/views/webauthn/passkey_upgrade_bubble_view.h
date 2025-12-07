// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PASSKEY_UPGRADE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PASSKEY_UPGRADE_BUBBLE_VIEW_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"

class PasswordBubbleControllerBase;
class PasskeyUpgradeBubbleController;
class RichHoverButton;

namespace content {
class WebContents;
}

namespace views {
class BoxLayoutView;
}  // namespace views

// A bubble informing the user that a passkey was created during a passkey
// upgrade request (conditional create).
class PasskeyUpgradeBubbleView : public PasswordBubbleViewBase {
  METADATA_HEADER(PasskeyUpgradeBubbleView, PasswordBubbleViewBase)

 public:
  PasskeyUpgradeBubbleView(content::WebContents* web_contents,
                           views::BubbleAnchor anchor,
                           DisplayReason display_reason,
                           std::string passkey_rp_id);
  PasskeyUpgradeBubbleView(const PasskeyUpgradeBubbleView&) = delete;
  PasskeyUpgradeBubbleView& operator=(const PasskeyUpgradeBubbleView&) = delete;
  ~PasskeyUpgradeBubbleView() override;

  RichHoverButton* manage_passkeys_button_for_testing();

 private:
  void ManagePasskeysButtonClicked();

  // PasswordBubbleViewBase:
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;
  ui::ImageModel GetWindowIcon() override;

  std::unique_ptr<PasskeyUpgradeBubbleController> controller_;

  raw_ptr<views::BoxLayoutView> container_ = nullptr;
  raw_ptr<RichHoverButton> manage_passkeys_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PASSKEY_UPGRADE_BUBBLE_VIEW_H_
