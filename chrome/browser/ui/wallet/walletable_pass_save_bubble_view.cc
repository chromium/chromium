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

using enum optimization_guide::proto::WalletablePass::PassCase;
using optimization_guide::proto::EventPass;
using optimization_guide::proto::LoyaltyCard;

std::unique_ptr<views::BoxLayoutView> GetAttributesContainer() {
  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetBetweenChildSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_CONTENT_LIST_VERTICAL_SINGLE))
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetAccessibleRole(ax::mojom::Role::kDescriptionList)
      .Build();
}
}  // namespace

WalletablePassSaveBubbleView::WalletablePassSaveBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    WalletablePassSaveBubbleController* controller)
    : WalletablePassBubbleViewBase(anchor_view, web_contents, controller),
      controller_(controller->GetWeakPtr()) {
  set_fixed_width(autofill::kAutofillAiBubbleWidth);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  set_margins(autofill::GetAutofillAiBubbleInnerMargins());
  SetAccessibleTitle(l10n_util::GetStringUTF16(GetDialogTitleResourceId()));

  auto* main_content_wrapper =
      AddChildView(views::Builder<views::BoxLayoutView>()
                       .SetOrientation(views::BoxLayout::Orientation::kVertical)
                       .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
                       .Build());

  std::unique_ptr<views::BoxLayoutView> subtitle_container =
      autofill::CreateAutofillAiBubbleSubtitleContainer();
  subtitle_container->AddChildView(GetSubtitleLabel());
  main_content_wrapper->AddChildView(std::move(subtitle_container));

  AddChildView(GetAttributesView());

  SetShowCloseButton(true);
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(
                     IDS_WALLET_WALLETABLE_PASS_SAVE_DIALOG_SAVE_BUTTON));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(
                     IDS_WALLET_WALLETABLE_PASS_SAVE_DIALOG_NO_THANKS_BUTTON));
}

WalletablePassSaveBubbleView::~WalletablePassSaveBubbleView() = default;

std::unique_ptr<views::BoxLayoutView>
WalletablePassSaveBubbleView::GetAttributesView() {
  switch (controller_->pass().pass_case()) {
    case kLoyaltyCard:
      return GetLoyaltyCardAttributesView();
    case kEventPass:
      return GetEventPassAttributesView();
    case PASS_NOT_SET:
      NOTREACHED() << "Not supported walletable pass type: "
                   << controller_->pass().pass_case();
  }
}

std::unique_ptr<views::BoxLayoutView>
WalletablePassSaveBubbleView::GetLoyaltyCardAttributesView() {
  std::unique_ptr<views::BoxLayoutView> container = GetAttributesContainer();
  LoyaltyCard loyalty_card = controller_->pass().loyalty_card();

  if (loyalty_card.has_issuer_name()) {
    container->AddChildView(autofill::CreateAutofillAiBubbleAttributeRow(
        l10n_util::GetStringUTF16(
            IDS_WALLET_WALLETABLE_PASS_LOYALTY_CARD_ISSUER_ATTRIBUTE_NAME),
        base::UTF8ToUTF16(loyalty_card.issuer_name())));
  }
  if (loyalty_card.has_member_id()) {
    container->AddChildView(autofill::CreateAutofillAiBubbleAttributeRow(
        l10n_util::GetStringUTF16(
            IDS_WALLET_WALLETABLE_PASS_LOYALTY_CARD_MEMBER_ID_ATTRIBUTE_NAME),
        base::UTF8ToUTF16(loyalty_card.member_id())));
  }
  return container;
}

std::unique_ptr<views::BoxLayoutView>
WalletablePassSaveBubbleView::GetEventPassAttributesView() {
  std::unique_ptr<views::BoxLayoutView> container = GetAttributesContainer();
  EventPass event_pass = controller_->pass().event_pass();

  if (event_pass.has_event_name()) {
    container->AddChildView(autofill::CreateAutofillAiBubbleAttributeRow(
        l10n_util::GetStringUTF16(
            IDS_WALLET_WALLETABLE_PASS_EVENT_PASS_EVENT_NAME_ATTRIBUTE_NAME),
        base::UTF8ToUTF16(event_pass.event_name())));
  }
  if (event_pass.has_issuer_name()) {
    container->AddChildView(autofill::CreateAutofillAiBubbleAttributeRow(
        l10n_util::GetStringUTF16(
            IDS_WALLET_WALLETABLE_PASS_EVENT_PASS_ISSUER_ATTRIBUTE_NAME),
        base::UTF8ToUTF16(event_pass.issuer_name())));
  }
  if (event_pass.has_venue()) {
    container->AddChildView(autofill::CreateAutofillAiBubbleAttributeRow(
        l10n_util::GetStringUTF16(
            IDS_WALLET_WALLETABLE_PASS_EVENT_PASS_VENUE_ATTRIBUTE_NAME),
        base::UTF8ToUTF16(event_pass.venue())));
  }
  if (event_pass.has_event_start_date()) {
    container->AddChildView(autofill::CreateAutofillAiBubbleAttributeRow(
        l10n_util::GetStringUTF16(
            IDS_WALLET_WALLETABLE_PASS_EVENT_PASS_DATE_ATTRIBUTE_NAME),
        base::UTF8ToUTF16(event_pass.event_start_date())));
  }
  return container;
}

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
  switch (controller_->pass().pass_case()) {
    case kLoyaltyCard:
      return IDS_WALLET_WALLETABLE_PASS_SAVE_LOYALTY_CARD_DIALOG_TITLE;
    case kEventPass:
      return IDS_WALLET_WALLETABLE_PASS_SAVE_EVENT_TICKET_DIALOG_TITLE;
    case PASS_NOT_SET:
      NOTREACHED() << "Not supported walletable pass type: "
                   << controller_->pass().pass_case();
  }
}

int WalletablePassSaveBubbleView::GetHeaderImageResourceId() const {
  switch (controller_->pass().pass_case()) {
    case kLoyaltyCard:
      return IDR_WALLET_PASS_SAVE_LOYALTY_CARD_LOTTIE;
    case kEventPass:
      return IDR_WALLET_PASS_SAVE_EVENT_TICKET_LOTTIE;
    case PASS_NOT_SET:
      NOTREACHED() << "Not supported walletable pass type: "
                   << controller_->pass().pass_case();
  }
}

void WalletablePassSaveBubbleView::OnGoToWalletClicked() {
  // TODO(crbug.com/451833977): Implement the go to Wallet action.
}

BEGIN_METADATA(WalletablePassSaveBubbleView)
END_METADATA

}  // namespace wallet
