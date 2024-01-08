// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MOVE_TO_ACCOUNT_STORE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MOVE_TO_ACCOUNT_STORE_BUBBLE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/passwords/bubble_controllers/move_to_account_store_bubble_controller.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "ui/base/metadata/metadata_header_macros.h"

// Bubble asking the user to move a profile credential to their account store.
class MoveToAccountStoreBubbleView : public PasswordBubbleViewBase {
  METADATA_HEADER(MoveToAccountStoreBubbleView, PasswordBubbleViewBase)

  class MovingBannerView;

 public:
  explicit MoveToAccountStoreBubbleView(content::WebContents* web_contents,
                                        views::View* anchor_view);
  ~MoveToAccountStoreBubbleView() override;

 private:
  // PasswordBubbleViewBase
  void AddedToWidget() override;
  MoveToAccountStoreBubbleController* GetController() override;
  const MoveToAccountStoreBubbleController* GetController() const override;
  ui::ImageModel GetWindowIcon() override;

  // Called when the favicon is loaded. If |favicon| isn't empty, it updates
  // |favicon| in |moving_banner_|
  void OnFaviconReady(const gfx::Image& favicon);

  // Holds a pointer to the banner illustarting that a password is being moved
  // from the device to the account.
  raw_ptr<MovingBannerView> moving_banner_;

  MoveToAccountStoreBubbleController controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MOVE_TO_ACCOUNT_STORE_BUBBLE_VIEW_H_
