// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/one_time_tokens/gmail_otp_opt_in_bubble_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/grit/browser_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

namespace {
constexpr int kBubbleWidth = 320;
}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(GmailOtpOptInBubbleView, kTurnOnButtonId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(GmailOtpOptInBubbleView,
                                      kNoThanksButtonId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(GmailOtpOptInBubbleView, kCloseButtonId);

GmailOtpOptInBubbleView::GmailOtpOptInBubbleView(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents,
    const std::u16string& account_email,
    base::RepeatingClosure learn_more_link_callback)
    : AutofillLocationBarBubble(anchor, web_contents) {
  set_fixed_width(kBubbleWidth);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetShowCloseButton(true);
  SetTitle(IDS_AUTOFILL_GMAIL_OTP_OPT_IN_TITLE);
  SetShowIcon(true);
  SetIcon(ui::ImageModel::FromVectorIcon(
      GooglePasswordManagerVectorIcon(), ui::kColorIcon,
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE)));

  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_GMAIL_OTP_OPT_IN_TURN_ON_BUTTON));
  SetButtonStyle(ui::mojom::DialogButton::kOk, ui::ButtonStyle::kProminent);
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(
                     IDS_AUTOFILL_GMAIL_OTP_OPT_IN_NO_THANKS_BUTTON));
  SetButtonStyle(ui::mojom::DialogButton::kCancel, ui::ButtonStyle::kTonal);

  const std::u16string link_text =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_GMAIL_OTP_OPT_IN_LEARN_MORE_LINK);
  std::vector<size_t> offsets;
  const std::u16string description_text =
      l10n_util::GetStringFUTF16(IDS_AUTOFILL_GMAIL_OTP_OPT_IN_DESCRIPTION,
                                 account_email, link_text, &offsets);

  description_label_ = AddChildView(
      views::Builder<views::StyledLabel>()
          .SetText(description_text)
          .SetDefaultTextStyle(views::style::STYLE_BODY_4)
          .SetDefaultEnabledColorId(ui::kColorSysOnSurfaceSubtle)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .AddStyleRange(
              gfx::Range(offsets[1], offsets[1] + link_text.length()),
              views::StyledLabel::RangeStyleInfo::CreateForLink(
                  std::move(learn_more_link_callback)))
          .Build());
}

GmailOtpOptInBubbleView::~GmailOtpOptInBubbleView() = default;

void GmailOtpOptInBubbleView::Hide() {
  CloseBubble();
}

void GmailOtpOptInBubbleView::OnWidgetInitialized() {
  AutofillLocationBarBubble::OnWidgetInitialized();

  // IDR_AUTOFILL_GMAIL_OTP_OPT_IN_HEADER is a static (non-animated) themed
  // Lottie illustration.
  auto image_view = std::make_unique<views::ImageView>(
      ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          IDR_AUTOFILL_GMAIL_OTP_OPT_IN_HEADER));
  image_view->GetViewAccessibility().SetIsIgnored(true);
  GetBubbleFrameView()->SetHeaderView(std::move(image_view));

  if (views::LabelButton* ok_button = GetOkButton()) {
    ok_button->SetProperty(views::kElementIdentifierKey, kTurnOnButtonId);
  }
  if (views::LabelButton* cancel_button = GetCancelButton()) {
    cancel_button->SetProperty(views::kElementIdentifierKey, kNoThanksButtonId);
  }
  if (views::Button* close_button = GetBubbleFrameView()->close_button()) {
    close_button->SetProperty(views::kElementIdentifierKey, kCloseButtonId);
  }
}

BEGIN_METADATA(GmailOtpOptInBubbleView)
END_METADATA

}  // namespace autofill
