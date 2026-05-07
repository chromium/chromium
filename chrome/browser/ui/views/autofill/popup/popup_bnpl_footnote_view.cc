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
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_utils.h"

namespace autofill {

namespace {
constexpr int kSelectionBorderThickness = 1;
}

PopupBnplFootnoteView::PopupBnplFootnoteView(
    base::WeakPtr<AutofillPopupController> controller,
    PopupRowView::AccessibilitySelectionDelegate& a11y_selection_delegate,
    base::RepeatingCallback<void(const std::u16string&, bool)>
        announce_callback)
    : a11y_selection_delegate_(a11y_selection_delegate) {
  controller_ = controller;
  announce_callback_ = std::move(announce_callback);
  // TODO(crbug.com/477689220): Investigate better approaches to manage the
  // layout that doesn't rely on setting custom margins, and instead
  // reuses underlying code.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(PopupBaseView::GetCornerRadius(),
                      PopupBaseView::ArrowHorizontalMargin())));

  SetBackground(views::CreateSolidBackground(ui::kColorBubbleFooterBackground));

  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);

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

  full_text_ = text_with_link.text;

  auto styled_label = std::make_unique<views::StyledLabel>();
  styled_label->SetText(full_text_);
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
          base::BindRepeating(&PopupBnplFootnoteView::ActivateSettingsLink,
                              weak_ptr_factory_.GetWeakPtr()));
  link_style.text_style = views::style::STYLE_LINK_5;
  styled_label->AddStyleRange(text_with_link.offset, link_style);

  AddChildView(std::move(styled_label));

  if (auto* link = GetSettingsLink()) {
    link->SetBorder(views::CreateEmptyBorder(kSelectionBorderThickness));
  }
}

PopupBnplFootnoteView::~PopupBnplFootnoteView() = default;

views::Link* PopupBnplFootnoteView::GetSettingsLink() const {
  for (views::View* child : children()) {
    if (auto* styled_label = views::AsViewClass<views::StyledLabel>(child)) {
      for (views::View* label_child : styled_label->children()) {
        if (auto* link = views::AsViewClass<views::Link>(label_child)) {
          return link;
        }
      }
    }
  }
  return nullptr;
}

void PopupBnplFootnoteView::FocusSettingsLink() {
  if (auto* link = GetSettingsLink()) {
    is_settings_link_selected_ = true;
    link->SetBorder(views::CreateSolidBorder(kSelectionBorderThickness,
                                             ui::kColorFocusableBorderFocused));
    a11y_selection_delegate_->NotifyAXSelection(*this);
    this->NotifyAccessibilityEventDeprecated(ax::mojom::Event::kFocus, true);
    this->GetViewAccessibility().SetIsSelected(true);
    announce_callback_.Run(std::u16string(link->GetText()), /*polite=*/false);
  }
}

bool PopupBnplFootnoteView::IsSettingsLinkFocused() const {
  return is_settings_link_selected_;
}

void PopupBnplFootnoteView::ActivateSettingsLink() {
  if (!controller_ || !controller_->GetWebContents()) {
    return;
  }

  Profile* profile = Profile::FromBrowserContext(
      controller_->GetWebContents()->GetBrowserContext());
  if (!profile) {
    return;
  }

  chrome::ShowSettingsSubPageForProfile(profile, chrome::kPaymentsSubPage);
}

void PopupBnplFootnoteView::UnfocusSettingsLink() {
  is_settings_link_selected_ = false;
  if (auto* link = GetSettingsLink()) {
    link->SetBorder(views::CreateEmptyBorder(kSelectionBorderThickness));
  }
  this->GetViewAccessibility().SetIsSelected(false);
}

BEGIN_METADATA(PopupBnplFootnoteView)
END_METADATA

}  // namespace autofill
