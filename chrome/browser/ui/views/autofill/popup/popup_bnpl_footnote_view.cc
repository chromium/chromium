// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_bnpl_footnote_view.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/common/webui_url_constants.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"

namespace autofill {

namespace {

void OnSettingsLinkClicked(base::WeakPtr<AutofillPopupController> controller) {
  if (!controller || !controller->GetWebContents()) {
    return;
  }

  Profile* profile = Profile::FromBrowserContext(
      controller->GetWebContents()->GetBrowserContext());
  if (!profile) {
    return;
  }

  chrome::ShowSettingsSubPageForProfile(profile, chrome::kPaymentsSubPage);
}

}  // namespace

PopupBnplFootnoteView::PopupBnplFootnoteView(
    base::WeakPtr<AutofillPopupController> controller) {
  // TODO(crbug.com/477689220): Investigate better approaches to manage the
  // layout that doesn't rely on setting custom margins, and instead
  // reuses underlying code.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(PopupBaseView::GetCornerRadius(),
                      PopupBaseView::ArrowHorizontalMargin())));

  SetBackground(views::CreateSolidBackground(ui::kColorBubbleFooterBackground));

  if (!controller || !controller->GetWebContents()) {
    return;
  }

  PersonalDataManager* pdm = PersonalDataManagerFactory::GetForBrowserContext(
      controller->GetWebContents()->GetBrowserContext());
  if (!pdm) {
    return;
  }

  autofill::payments::TextWithLink text_with_link =
      autofill::payments::GetBnplUiFooterTextForAi(
          pdm->payments_data_manager());

  auto styled_label = std::make_unique<views::StyledLabel>();
  styled_label->SetText(text_with_link.text);
  styled_label->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  styled_label->SetDefaultTextStyle(views::style::STYLE_BODY_5);
  styled_label->SetDefaultEnabledColorId(ui::kColorLabelForegroundSecondary);
  styled_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  if (!text_with_link.bold_range.is_empty()) {
    views::StyledLabel::RangeStyleInfo bold_style;
    bold_style.text_style = views::style::STYLE_BODY_5_BOLD;
    styled_label->AddStyleRange(text_with_link.bold_range, bold_style);
  }

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          base::BindRepeating(&OnSettingsLinkClicked, controller));
  link_style.text_style = views::style::STYLE_LINK_5;
  styled_label->AddStyleRange(text_with_link.offset, link_style);

  AddChildView(std::move(styled_label));
}

PopupBnplFootnoteView::~PopupBnplFootnoteView() = default;

gfx::Size PopupBnplFootnoteView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size = views::View::CalculatePreferredSize(available_size);
  // Prevent the footnote from forcing the popup to `kAutofillPopupMaxWidth`.
  // By returning a width of 0 when unbounded, the layout manager will size
  // the popup based strictly on the other rows (like the BNPL providers) and
  // then pass that determined width back to this view to calculate its size.
  if (!available_size.width().is_bounded()) {
    size.set_width(0);
  }
  return size;
}

BEGIN_METADATA(PopupBnplFootnoteView)
END_METADATA

}  // namespace autofill
