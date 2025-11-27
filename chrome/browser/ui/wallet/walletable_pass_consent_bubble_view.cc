// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/wallet/walletable_pass_consent_bubble_view.h"

#include "chrome/browser/ui/views/autofill/autofill_bubble_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/wallet/walletable_pass_consent_bubble_controller.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model_utils.h"
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

using enum optimization_guide::proto::PassCategory;

constexpr int kBubbleWidth = 320;
constexpr int kSubTitleBottomMargin = 16;

std::unique_ptr<views::BoxLayoutView> GetSubtitleDescriptionContainer() {
  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetInsideBorderInsets(gfx::Insets::TLBR(0, 0, kSubTitleBottomMargin, 0))
      .Build();
}
}  // namespace

WalletablePassConsentBubbleView::WalletablePassConsentBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    WalletablePassConsentBubbleController* controller)
    : WalletablePassBubbleViewBase(anchor_view, web_contents, controller),
      pass_category_(controller->pass_category()),
      controller_(controller->GetWeakPtr()) {
  set_fixed_width(kBubbleWidth);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  SetAccessibleTitle(l10n_util::GetStringUTF16(
      IDS_WALLET_WALLETABLE_PASS_CONSENT_DIALOG_TITLE));

  auto* main_content_wrapper =
      AddChildView(views::Builder<views::BoxLayoutView>()
                       .SetOrientation(views::BoxLayout::Orientation::kVertical)
                       .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
                       .Build());

  std::unique_ptr<views::BoxLayoutView> subtitle_description_container =
      GetSubtitleDescriptionContainer();

  subtitle_description_container->AddChildView(GetSubtitleDescriptionLabel());
  main_content_wrapper->AddChildView(std::move(subtitle_description_container));
  main_content_wrapper->AddChildView(GetSubtitleActionLabel());

  SetShowCloseButton(true);
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(
                     IDS_WALLET_WALLETABLE_PASS_CONSENT_DIALOG_TURN_ON_BUTTON));
  SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(
          IDS_WALLET_WALLETABLE_PASS_CONSENT_DIALOG_NO_THANKS_BUTTON));
}

WalletablePassConsentBubbleView::~WalletablePassConsentBubbleView() = default;

std::unique_ptr<views::StyledLabel>
WalletablePassConsentBubbleView::GetSubtitleDescriptionLabel() {
  const std::u16string description = l10n_util::GetStringUTF16(
      IDS_WALLET_WALLETABLE_PASS_CONSENT_DIALOG_SUBTITLE_DESCRIPTION);
  return views::Builder<views::StyledLabel>()
      .SetText(description)
      .SetDefaultTextStyle(views::style::STYLE_BODY_4)
      .SetDefaultEnabledColorId(ui::kColorSysOnSurfaceSubtle)
      .SetAccessibleRole(ax::mojom::Role::kDetails)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .Build();
}

std::unique_ptr<views::StyledLabel>
WalletablePassConsentBubbleView::GetSubtitleActionLabel() {
  std::vector<size_t> offsets;
  const std::u16string learn_more_text = l10n_util::GetStringUTF16(
      IDS_WALLET_WALLETABLE_PASS_CONSENT_DIALOG_LEARN_MORE);
  std::u16string formatted_text = l10n_util::GetStringFUTF16(
      IDS_WALLET_WALLETABLE_PASS_CONSENT_DIALOG_SUBTITLE_ACTION,
      {learn_more_text}, &offsets);

  gfx::Range learn_more_range(offsets[0], offsets[0] + learn_more_text.size());
  auto learn_more =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &WalletablePassConsentBubbleController::OnLearnMoreClicked,
          controller_));

  return views::Builder<views::StyledLabel>()
      .SetText(std::move(formatted_text))
      .SetDefaultTextStyle(views::style::STYLE_BODY_4)
      .SetDefaultEnabledColorId(ui::kColorSysOnSurfaceSubtle)
      .SetAccessibleRole(ax::mojom::Role::kDetails)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .AddStyleRange(learn_more_range, learn_more)
      .Build();
}

void WalletablePassConsentBubbleView::AddedToWidget() {
  // Set header view
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  // TODO(crbug.com/445826875): Replace with wallet-specific lottie image.
  std::unique_ptr<views::ImageView> image_view =
      std::make_unique<views::ImageView>(
          bundle.GetThemedLottieImageNamed(GetHeaderImageResourceId()));
  image_view->GetViewAccessibility().SetIsInvisible(true);

  GetBubbleFrameView()->SetHeaderView(std::move(image_view));

  // Set title view
  GetBubbleFrameView()->SetTitleView(
      autofill::CreateWalletBubbleTitleView(l10n_util::GetStringUTF16(
          IDS_WALLET_WALLETABLE_PASS_CONSENT_DIALOG_TITLE)));
}

int WalletablePassConsentBubbleView::GetHeaderImageResourceId() const {
  switch (pass_category_) {
    case PASS_CATEGORY_LOYALTY_CARD:
      return IDR_WALLET_PASS_SAVE_LOYALTY_CARD_LOTTIE;
    case PASS_CATEGORY_EVENT_PASS:
      return IDR_WALLET_PASS_SAVE_EVENT_TICKET_LOTTIE;
    case PASS_CATEGORY_TRANSIT_TICKET:
      return IDR_WALLET_PASS_SAVE_TRANSPORT_TICKET_LOTTIE;
    case PASS_CATEGORY_UNSPECIFIED:
    default:
      NOTREACHED() << "Not supported walletable pass category: "
                   << pass_category_;
  }
}

BEGIN_METADATA(WalletablePassConsentBubbleView)
END_METADATA

}  // namespace wallet
