// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SYNC_DICE_SIGNIN_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SYNC_DICE_SIGNIN_BUTTON_VIEW_H_

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view.h"

// Sign-in button view used for Desktop Identity Consistency that presents the
// account information (avatar image and email) and allows the user to
// sign in to Chrome or to enable sync.
//
// The button also presents on the right hand side a drown-down arrow button
// that the user can interact with.
class DiceSigninButtonView : public views::View {
 public:
  // Create a non-personalized sign-in button.
  // |button_listener| is called every time the user interacts with this button.
  // The button is prominent by default but can be made non-prominent by setting
  // |prominent| to false.
  explicit DiceSigninButtonView(views::ButtonListener* button_listener,
                                bool prominent = true);

  // Creates a sign-in button personalized with the data from |account|.
  // |button_listener| will be called for events originating from |this| or from
  // |drop_down_arrow|. The drop down arrow will only be shown if
  // |show_drop_down_arrow| is true.
  DiceSigninButtonView(const AccountInfo& account_info,
                       const gfx::Image& account_icon,
                       views::ButtonListener* button_listener,
                       bool use_account_name_as_title = false);
  ~DiceSigninButtonView() override;

  views::LabelButton* signin_button() const { return signin_button_; }
  base::Optional<AccountInfo> account() const { return account_; }

 private:

  views::LabelButton* signin_button_ = nullptr;

  const base::Optional<AccountInfo> account_;

  DISALLOW_COPY_AND_ASSIGN(DiceSigninButtonView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SYNC_DICE_SIGNIN_BUTTON_VIEW_H_
