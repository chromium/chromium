// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SYNC_BUBBLE_SYNC_PROMO_SIGNIN_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SYNC_BUBBLE_SYNC_PROMO_SIGNIN_BUTTON_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

// Sign-in button view used by Sync promos that presents the
// account information (avatar image and email) and allows the user to
// sign in to Chrome or to enable sync.
class BubbleSyncPromoSigninButtonView : public views::View {
 public:
  METADATA_HEADER(BubbleSyncPromoSigninButtonView);
  // Create a non-personalized sign-in button.
  // |callback| is called every time the user interacts with this button.
  // The button is prominent by default but can be made non-prominent by setting
  // |prominent| to false.
  explicit BubbleSyncPromoSigninButtonView(
      views::Button::PressedCallback callback,
      bool prominent = true);

  // Creates a sign-in button personalized with the data from |account|.
  // |callback| is called every time the user interacts with this button.
  BubbleSyncPromoSigninButtonView(const AccountInfo& account_info,
                                  const gfx::Image& account_icon,
                                  views::Button::PressedCallback callback,
                                  bool use_account_name_as_title = false);
  BubbleSyncPromoSigninButtonView(const BubbleSyncPromoSigninButtonView&) =
      delete;
  BubbleSyncPromoSigninButtonView& operator=(
      const BubbleSyncPromoSigninButtonView&) = delete;
  ~BubbleSyncPromoSigninButtonView() override;

  absl::optional<AccountInfo> account() const { return account_; }

 private:
  const absl::optional<AccountInfo> account_;
};

BEGIN_VIEW_BUILDER(, BubbleSyncPromoSigninButtonView, views::View)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(, BubbleSyncPromoSigninButtonView)

#endif  // CHROME_BROWSER_UI_VIEWS_SYNC_BUBBLE_SYNC_PROMO_SIGNIN_BUTTON_VIEW_H_
