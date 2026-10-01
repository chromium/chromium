// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_footer_with_link_view.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_utils.h"

namespace autofill {

namespace {
constexpr int kChromeRefreshIconSize = 20;
constexpr int kRightPadding = 8;
}  // namespace

PopupFooterWithLinkView::PopupFooterWithLinkView(
    base::WeakPtr<AutofillPopupController> controller,
    PopupRowView::AccessibilitySelectionDelegate& a11y_selection_delegate,
    int text_id,
    int link_text_id,
    std::string_view settings_subpage,
    const gfx::VectorIcon* icon)
    : controller_(controller),
      a11y_selection_delegate_(a11y_selection_delegate),
      settings_subpage_(settings_subpage) {
  if (!controller_ || !controller_->GetWebContents()) {
    return;
  }

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::TLBR(PopupBaseView::GetCornerRadius(),
                        PopupBaseView::ArrowHorizontalMargin(),
                        PopupBaseView::GetCornerRadius(), kRightPadding),
      PopupBaseView::ArrowHorizontalMargin()));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  SetBackground(views::CreateSolidBackground(ui::kColorDropdownBackground));

  if (icon) {
    AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            *icon, ui::kColorIcon, kChromeRefreshIconSize)));
  }

  styled_label_ = AddChildView(std::make_unique<views::StyledLabel>());
  layout->SetFlexForView(styled_label_, 1);
  std::vector<size_t> offsets;
  std::u16string link_text = l10n_util::GetStringUTF16(link_text_id);
  std::u16string formatted_text =
      l10n_util::GetStringFUTF16(text_id, {link_text}, &offsets);
  styled_label_->SetText(formatted_text);
  styled_label_->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  styled_label_->SetDefaultTextStyle(views::style::STYLE_BODY_4);
  styled_label_->SetDefaultEnabledColorId(ui::kColorSysOnSurfaceSubtle);
  styled_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  if (offsets.size() == 1) {
    views::StyledLabel::RangeStyleInfo link_style =
        views::StyledLabel::RangeStyleInfo::CreateForLink(
            base::BindRepeating(&PopupFooterWithLinkView::OnLinkClicked,
                                weak_ptr_factory_.GetWeakPtr()));
    link_style.text_style = views::style::STYLE_LINK_4;
    styled_label_->AddStyleRange(
        gfx::Range(offsets[0], offsets[0] + link_text.length()), link_style);
  }

  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetName(formatted_text);
}

PopupFooterWithLinkView::~PopupFooterWithLinkView() = default;

void PopupFooterWithLinkView::OnLinkClicked() {
  if (!controller_ || !controller_->GetWebContents()) {
    return;
  }

  Profile* profile = Profile::FromBrowserContext(
      controller_->GetWebContents()->GetBrowserContext());
  if (!profile) {
    return;
  }

  chrome::ShowSettingsSubPageForProfile(profile, settings_subpage_);
}

void PopupFooterWithLinkView::Layout(views::View::PassKey pass_key) {
  LayoutSuperclass<PopupInteractiveRowView>(this);

  // Set focus behavior to `FocusBehavior::NEVER` after layout (since
  // `views::StyledLabel` creates links lazily) to prevent clicks from
  // stealing native focus from the search bar.
  for (auto* link : GetSettingsLinks()) {
    link->SetFocusBehavior(views::View::FocusBehavior::NEVER);
    if (views::FocusRing* focus_ring = views::FocusRing::Get(link)) {
      focus_ring->SetOutsetFocusRingDisabled(true);
      focus_ring->SetHaloInset(0);
      focus_ring->SetHasFocusPredicate(base::BindRepeating(
          [](base::WeakPtr<PopupFooterWithLinkView> view,
             const views::View* host) {
            return view && view->GetSelectedCell().has_value();
          },
          weak_ptr_factory_.GetWeakPtr()));
      focus_ring->Refresh();
    }
  }
}

std::vector<views::Link*> PopupFooterWithLinkView::GetSettingsLinks() const {
  std::vector<views::Link*> links;
  if (!styled_label_) {
    return links;
  }
  for (views::View* child : styled_label_->children()) {
    if (auto* link = views::AsViewClass<views::Link>(child)) {
      links.push_back(link);
    }
  }
  return links;
}

std::optional<PopupInteractiveRowView::CellType>
PopupFooterWithLinkView::GetSelectedCell() const {
  return selected_cell_;
}

void PopupFooterWithLinkView::SetSelectedCell(std::optional<CellType> cell) {
  if (selected_cell_ == cell) {
    return;
  }
  selected_cell_ = cell;

  for (auto* link : GetSettingsLinks()) {
    if (auto* focus_ring = views::FocusRing::Get(link)) {
      focus_ring->Refresh();
    }
  }
  if (selected_cell_.has_value()) {
    a11y_selection_delegate_->NotifyAXSelection(*this);
    NotifyAccessibilityEventDeprecated(ax::mojom::Event::kFocus, true);
    GetViewAccessibility().SetIsSelected(true);
  } else {
    GetViewAccessibility().SetIsSelected(false);
  }
}

bool PopupFooterWithLinkView::HandleKeyPressEvent(
    const input::NativeWebKeyboardEvent& event) {
  if (event.windows_key_code == ui::VKEY_RETURN ||
      event.windows_key_code == ui::VKEY_SPACE) {
    OnLinkClicked();
    return true;
  }
  return false;
}

bool PopupFooterWithLinkView::IsSelectable() const {
  return true;
}

BEGIN_METADATA(PopupFooterWithLinkView)
END_METADATA

std::unique_ptr<PopupFooterWithLinkView> CreateAtMemoryAiDisclosureView(
    base::WeakPtr<AutofillPopupController> controller,
    PopupRowView::AccessibilitySelectionDelegate& a11y_selection_delegate) {
  return std::make_unique<PopupFooterWithLinkView>(
      std::move(controller), a11y_selection_delegate,
      IDS_AUTOFILL_AT_MEMORY_AI_DISCLOSURE,
      IDS_AUTOFILL_AT_MEMORY_AI_DISCLOSURE_LINK,
      chrome::kSuggestionsFromGeminiSubPage, /*icon=*/nullptr);
}

}  // namespace autofill
