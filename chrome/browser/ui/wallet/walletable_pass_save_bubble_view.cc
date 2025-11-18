// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/wallet/walletable_pass_save_bubble_view.h"

#include "chrome/browser/ui/views/autofill/autofill_bubble_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/wallet/walletable_pass_save_bubble_controller.h"
#include "chrome/grit/browser_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/style/typography.h"

namespace wallet {
namespace {
constexpr int kBubbleWidth = 320;
constexpr int kSubTitleBottomMargin = 16;

std::unique_ptr<views::BoxLayoutView> GetSubtitleContainer() {
  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetInsideBorderInsets(gfx::Insets::TLBR(0, 0, kSubTitleBottomMargin, 0))
      .Build();
}
}  // namespace

WalletablePassSaveBubbleView::WalletablePassSaveBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    WalletablePassSaveBubbleController* controller)
    : WalletablePassBubbleViewBase(anchor_view, web_contents, controller),
      pass_(controller->pass()) {
  set_fixed_width(kBubbleWidth);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  SetAccessibleTitle(l10n_util::GetStringUTF16(GetDialogTitleResourceId()));

  auto* main_content_wrapper =
      AddChildView(views::Builder<views::BoxLayoutView>()
                       .SetOrientation(views::BoxLayout::Orientation::kVertical)
                       .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
                       .Build());

  std::unique_ptr<views::BoxLayoutView> subtitle_container =
      GetSubtitleContainer();
  subtitle_container->AddChildView(GetSubtitleLabel());
  main_content_wrapper->AddChildView(std::move(subtitle_container));
  // TODO(crbug.com/451833977): Add pass attributes to the bubble view.

  SetShowCloseButton(true);
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(
                     IDS_WALLET_WALLETABLE_PASS_SAVE_DIALOG_SAVE_BUTTON));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(
                     IDS_WALLET_WALLETABLE_PASS_SAVE_DIALOG_NO_THANKS_BUTTON));
}

WalletablePassSaveBubbleView::~WalletablePassSaveBubbleView() = default;

void WalletablePassSaveBubbleView::AddedToWidget() {
  // Set header view
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  std::unique_ptr<views::ImageView> image_view =
      std::make_unique<views::ImageView>(
          bundle.GetThemedLottieImageNamed(GetHeaderImageResourceId()));
  image_view->GetViewAccessibility().SetIsInvisible(true);
  GetBubbleFrameView()->SetHeaderView(std::move(image_view));

  // Set title view
  GetBubbleFrameView()->SetTitleView(autofill::CreateWalletBubbleTitleView(
      l10n_util::GetStringUTF16(GetDialogTitleResourceId())));
}

std::unique_ptr<views::StyledLabel>
WalletablePassSaveBubbleView::GetSubtitleLabel() {
  std::vector<size_t> offsets;
  const std::u16string google_wallet_text =
      l10n_util::GetStringUTF16(IDS_WALLET_WALLETABLE_PASS_GOOGLE_WALLET_TITLE);

  // TODO(crbug.com/451833977): Replace the email with the actual user's email.
  std::u16string formatted_text = l10n_util::GetStringFUTF16(
      IDS_WALLET_WALLETABLE_PASS_SAVE_DIALOG_SUBTITLE,
      {google_wallet_text, u"dummy@gmail.com"}, &offsets);

  gfx::Range go_to_wallet_range(offsets[0],
                                offsets[0] + google_wallet_text.size());
  auto go_to_wallet = views::StyledLabel::RangeStyleInfo::CreateForLink(
      base::BindRepeating(&WalletablePassSaveBubbleView::OnGoToWalletClicked,
                          base::Unretained(this)));

  return views::Builder<views::StyledLabel>()
      .SetText(std::move(formatted_text))
      .SetDefaultTextStyle(views::style::STYLE_BODY_4)
      .SetDefaultEnabledColorId(ui::kColorSysOnSurfaceSubtle)
      .SetAccessibleRole(ax::mojom::Role::kDetails)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .AddStyleRange(go_to_wallet_range, go_to_wallet)
      .Build();
}

int WalletablePassSaveBubbleView::GetDialogTitleResourceId() const {
  switch (pass_->pass_case()) {
    case optimization_guide::proto::WalletablePass::kLoyaltyCard:
      return IDS_WALLET_WALLETABLE_PASS_SAVE_LOYALTY_CARD_DIALOG_TITLE;
    case optimization_guide::proto::WalletablePass::kEventPass:
      return IDS_WALLET_WALLETABLE_PASS_SAVE_EVENT_TICKET_DIALOG_TITLE;
    case optimization_guide::proto::WalletablePass::PASS_NOT_SET:
      NOTREACHED() << "Not supported walletable pass type: "
                   << pass_->pass_case();
  }
}

int WalletablePassSaveBubbleView::GetHeaderImageResourceId() const {
  switch (pass_->pass_case()) {
    case optimization_guide::proto::WalletablePass::kLoyaltyCard:
      return IDR_WALLET_PASS_SAVE_LOYALTY_CARD_LOTTIE;
    case optimization_guide::proto::WalletablePass::kEventPass:
      return IDR_WALLET_PASS_SAVE_EVENT_TICKET_LOTTIE;
    case optimization_guide::proto::WalletablePass::PASS_NOT_SET:
      NOTREACHED() << "Not supported walletable pass type: "
                   << pass_->pass_case();
  }
}

void WalletablePassSaveBubbleView::OnGoToWalletClicked() {
  // TODO(crbug.com/451833977): Implement the go to Wallet action.
}

BEGIN_METADATA(WalletablePassSaveBubbleView)
END_METADATA

}  // namespace wallet
