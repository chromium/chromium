// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/bubble_sync_promo_view.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/sync/bubble_sync_promo_signin_button_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"

BubbleSyncPromoView::BubbleSyncPromoView(
    Profile* profile,
    BubbleSyncPromoDelegate* delegate,
    signin_metrics::AccessPoint access_point,
    int accounts_promo_message_resource_id,
    bool signin_button_prominent,
    int text_style)
    : delegate_(delegate) {
  DCHECK(!profile->IsGuestSession());
  AccountInfo account;
  // Signin promos can be shown in incognito, they use an empty account list.
  if (!profile->IsOffTheRecord())
    account = signin_ui_util::GetSingleAccountForPromos(profile);

  const views::LayoutOrientation orientation =
      (base::FeatureList::IsEnabled(features::kPowerBookmarksSidePanel) &&
       account.IsEmpty())
          ? views::LayoutOrientation::kHorizontal
          : views::LayoutOrientation::kVertical;

  std::unique_ptr<views::FlexLayout> layout =
      std::make_unique<views::FlexLayout>();
  layout->SetOrientation(orientation);
  layout->SetDefault(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred, true));
  SetLayoutManager(std::move(layout));

  // Always show the accounts promo message for now.
  const int title_resource_id = accounts_promo_message_resource_id;
  const int title_max_width = 218;

  if (title_resource_id) {
    std::u16string title_text = l10n_util::GetStringUTF16(title_resource_id);
    views::Label* title = new views::Label(
        title_text, views::style::CONTEXT_DIALOG_BODY_TEXT, text_style);
    title->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    title->SetMultiLine(true);
    if (orientation == views::LayoutOrientation::kHorizontal) {
      title->SetMaximumWidth(title_max_width);
    } else {
      title->SetProperty(
          views::kMarginsKey,
          gfx::Insets::TLBR(0, 0,
                            ChromeLayoutProvider::Get()
                                ->GetDialogInsetsForContentType(
                                    views::DialogContentType::kText,
                                    views::DialogContentType::kText)
                                .bottom(),
                            0));
    }
    AddChildView(title);
  }

  views::Button::PressedCallback callback = base::BindRepeating(
      &BubbleSyncPromoView::EnableSync, base::Unretained(this));

  std::unique_ptr<BubbleSyncPromoSigninButtonView> signin_button_pointer;

  if (account.IsEmpty()) {
    signin_button_pointer = std::make_unique<BubbleSyncPromoSigninButtonView>(
        std::move(callback), signin_button_prominent);
  } else {
    gfx::Image account_icon = account.account_image;
    if (account_icon.IsEmpty()) {
      account_icon = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          profiles::GetPlaceholderAvatarIconResourceID());
    }
    signin_button_pointer = std::make_unique<BubbleSyncPromoSigninButtonView>(
        account, account_icon, std::move(callback),
        /*use_account_name_as_title=*/true);
  }

  if (orientation == views::LayoutOrientation::kHorizontal) {
    views::View* button_parent = AddChildView(std::make_unique<views::View>());
    std::unique_ptr<views::FlexLayout> button_layout =
        std::make_unique<views::FlexLayout>();
    button_layout->SetOrientation(views::LayoutOrientation::kHorizontal);
    button_layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
    button_parent->SetLayoutManager(std::move(button_layout));
    button_parent->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kUnbounded));
    signin_button_pointer->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                 views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kUnbounded)
            .WithAlignment(views::LayoutAlignment::kEnd));
    signin_button_view_ =
        button_parent->AddChildView(std::move(signin_button_pointer));
  } else {
    signin_button_view_ = AddChildView(std::move(signin_button_pointer));
  }

  signin_metrics::RecordSigninImpressionUserActionForAccessPoint(access_point);
}

BubbleSyncPromoView::~BubbleSyncPromoView() = default;

void BubbleSyncPromoView::EnableSync() {
  absl::optional<AccountInfo> account = signin_button_view_->account();
  delegate_->OnEnableSync(account.value_or(AccountInfo()));
}

BEGIN_METADATA(BubbleSyncPromoView, views::View)
END_METADATA
