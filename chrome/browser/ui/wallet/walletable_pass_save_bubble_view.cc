// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/wallet/walletable_pass_save_bubble_view.h"

#include <variant>

#include "chrome/browser/ui/views/autofill/autofill_bubble_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/wallet/walletable_pass_save_bubble_controller.h"
#include "chrome/grit/browser_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "components/wallet/core/browser/data_models/walletable_pass.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
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
  subtitle_container->AddChildView(
      GetSubtitleLabel(controller_->GetPrimaryAccountEmail()));
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
  return std::visit(absl::Overload(
                        [&](const LoyaltyCard& loyalty_card) {
                          return GetLoyaltyCardAttributesView(loyalty_card);
                        },
                        [&](const EventPass& event_pass) {
                          return GetEventPassAttributesView(event_pass);
                        },
                        [&](const TransitTicket& transit_ticket) {
                          return GetTransitTicketAttributesView(transit_ticket);
                        },
                        [&](const BoardingPass& boarding_pass) {
                          // TODO(crbug.com/464993127): Implement BoardingPass
                          // UI.
                          return std::make_unique<views::BoxLayoutView>();
                        }),
                    controller_->pass().pass_data);
}

std::unique_ptr<views::BoxLayoutView>
WalletablePassSaveBubbleView::GetLoyaltyCardAttributesView(
    const LoyaltyCard& loyalty_card) {
  std::unique_ptr<views::BoxLayoutView> container = GetAttributesContainer();

  if (!loyalty_card.issuer_name.empty()) {
    container->AddChildView(autofill::CreateAutofillAiBubbleAttributeRow(
        l10n_util::GetStringUTF16(
            IDS_WALLET_WALLETABLE_PASS_LOYALTY_CARD_ISSUER_ATTRIBUTE_NAME),
        base::UTF8ToUTF16(loyalty_card.issuer_name)));
  }
  if (!loyalty_card.member_id.empty()) {
    container->AddChildView(autofill::CreateAutofillAiBubbleAttributeRow(
        l10n_util::GetStringUTF16(
            IDS_WALLET_WALLETABLE_PASS_LOYALTY_CARD_MEMBER_ID_ATTRIBUTE_NAME),
        base::UTF8ToUTF16(loyalty_card.member_id)));
  }
  return container;
}

std::unique_ptr<views::BoxLayoutView>
WalletablePassSaveBubbleView::GetEventPassAttributesView(
    const EventPass& event_pass) {
  std::unique_ptr<views::BoxLayoutView> container = GetAttributesContainer();

  if (!event_pass.event_name.empty()) {
    container->AddChildView(autofill::CreateAutofillAiBubbleAttributeRow(
        l10n_util::GetStringUTF16(
            IDS_WALLET_WALLETABLE_PASS_EVENT_PASS_EVENT_NAME_ATTRIBUTE_NAME),
        base::UTF8ToUTF16(event_pass.event_name)));
  }
  if (!event_pass.issuer_name.empty()) {
    container->AddChildView(autofill::CreateAutofillAiBubbleAttributeRow(
        l10n_util::GetStringUTF16(
            IDS_WALLET_WALLETABLE_PASS_EVENT_PASS_ISSUER_ATTRIBUTE_NAME),
        base::UTF8ToUTF16(event_pass.issuer_name)));
  }
  if (!event_pass.venue.empty()) {
    container->AddChildView(autofill::CreateAutofillAiBubbleAttributeRow(
        l10n_util::GetStringUTF16(
            IDS_WALLET_WALLETABLE_PASS_EVENT_PASS_VENUE_ATTRIBUTE_NAME),
        base::UTF8ToUTF16(event_pass.venue)));
  }
  if (!event_pass.event_start_date.empty()) {
    container->AddChildView(autofill::CreateAutofillAiBubbleAttributeRow(
        l10n_util::GetStringUTF16(
            IDS_WALLET_WALLETABLE_PASS_EVENT_PASS_DATE_ATTRIBUTE_NAME),
        base::UTF8ToUTF16(event_pass.event_start_date)));
  }
  return container;
}

std::unique_ptr<views::BoxLayoutView>
WalletablePassSaveBubbleView::GetTransitTicketAttributesView(
    const TransitTicket& transit_ticket) {
  std::unique_ptr<views::BoxLayoutView> container = GetAttributesContainer();

  if (!transit_ticket.agency_name.empty()) {
    container->AddChildView(autofill::CreateAutofillAiBubbleAttributeRow(
        l10n_util::GetStringUTF16(
            IDS_WALLET_WALLETABLE_PASS_TRANSPORT_TICKET_OPERATOR_ATTRIBUTE_NAME),
        base::UTF8ToUTF16(transit_ticket.agency_name)));
  }
  if (!transit_ticket.origin.empty() && !transit_ticket.destination.empty()) {
    container->AddChildView(autofill::CreateAutofillAiBubbleAttributeRow(
        l10n_util::GetStringUTF16(
            IDS_WALLET_WALLETABLE_PASS_TRANSPORT_TICKET_TRIP_ATTRIBUTE_NAME),
        l10n_util::GetStringFUTF16(
            IDS_WALLET_WALLETABLE_PASS_TRANSPORT_TICKET_TRIP_VALUE,
            base::UTF8ToUTF16(transit_ticket.origin),
            base::UTF8ToUTF16(transit_ticket.destination))));
  }

  if (!transit_ticket.date_of_travel.empty()) {
    container->AddChildView(autofill::CreateAutofillAiBubbleAttributeRow(
        l10n_util::GetStringUTF16(
            IDS_WALLET_WALLETABLE_PASS_TRANSPORT_TICKET_DATE_ATTRIBUTE_NAME),
        base::UTF8ToUTF16(transit_ticket.date_of_travel)));
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
WalletablePassSaveBubbleView::GetSubtitleLabel(
    const std::u16string& user_email) {
  std::vector<size_t> offsets;
  const std::u16string google_wallet_text =
      l10n_util::GetStringUTF16(IDS_WALLET_WALLETABLE_PASS_GOOGLE_WALLET_TITLE);

  std::u16string formatted_text = l10n_util::GetStringFUTF16(
      IDS_WALLET_WALLETABLE_PASS_SAVE_DIALOG_SUBTITLE,
      {google_wallet_text, user_email}, &offsets);

  gfx::Range go_to_wallet_range(offsets[0],
                                offsets[0] + google_wallet_text.size());
  auto go_to_wallet =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &WalletablePassSaveBubbleController::OnGoToWalletClicked,
          controller_));

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
  return std::visit(
      absl::Overload(
          [](const LoyaltyCard& loyalty_card) {
            return IDS_WALLET_WALLETABLE_PASS_SAVE_LOYALTY_CARD_DIALOG_TITLE;
          },
          [](const EventPass& event_pass) {
            return IDS_WALLET_WALLETABLE_PASS_SAVE_EVENT_TICKET_DIALOG_TITLE;
          },
          [](const TransitTicket& transit_ticket) {
            return IDS_WALLET_WALLETABLE_PASS_SAVE_TRANSPORT_TICKET_DIALOG_TITLE;
          },
          [](const BoardingPass& boarding_pass) -> int {
            // TODO(crbug.com/464993127): Implement BoardingPass UI.
            return IDS_WALLET_WALLETABLE_PASS_SAVE_TRANSPORT_TICKET_DIALOG_TITLE;
          }),
      controller_->pass().pass_data);
}

int WalletablePassSaveBubbleView::GetHeaderImageResourceId() const {
  return std::visit(absl::Overload(
                        [](const LoyaltyCard& loyalty_card) {
                          return IDR_WALLET_PASS_SAVE_LOYALTY_CARD_LOTTIE;
                        },
                        [](const EventPass& event_pass) {
                          return IDR_WALLET_PASS_SAVE_EVENT_TICKET_LOTTIE;
                        },
                        [](const TransitTicket& transit_ticket) {
                          return IDR_WALLET_PASS_SAVE_TRANSPORT_TICKET_LOTTIE;
                        },
                        [](const BoardingPass& boarding_pass) {
                          return IDR_WALLET_PASS_SAVE_BOARDING_PASS_LOTTIE;
                        }),
                    controller_->pass().pass_data);
}

BEGIN_METADATA(WalletablePassSaveBubbleView)
END_METADATA

}  // namespace wallet
