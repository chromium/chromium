// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_header_view.h"

#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

OmniboxHeaderView::OmniboxHeaderView(OmniboxPopupViewViews* popup_view,
                                     size_t model_index)
    : popup_view_(popup_view),
      model_index_(model_index),
      // Using base::Unretained is correct here. 'this' outlives the callback.
      mouse_enter_exit_handler_(
          base::BindRepeating(&OmniboxHeaderView::UpdateUI,
                              base::Unretained(this))) {
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  // This is the designer-provided spacing that matches the NTP Realbox.
  // TODO(khalidpeer): Update this spacing for realbox per CR23 guidelines.
  layout->set_between_child_spacing(0);

  header_label_ = AddChildView(std::make_unique<views::Label>());
  header_label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  const gfx::FontList& font =
      views::TypographyProvider::Get()
          .GetFont(CONTEXT_OMNIBOX_SECTION_HEADER, views::style::STYLE_PRIMARY)
          .DeriveWithWeight(gfx::Font::Weight::MEDIUM);
  header_label_->SetFontList(font);

  header_toggle_button_ = AddChildView(views::CreateVectorToggleImageButton(
      base::BindRepeating(&OmniboxHeaderView::HeaderToggleButtonPressed,
                          base::Unretained(this))));
  mouse_enter_exit_handler_.ObserveMouseEnterExitOn(header_toggle_button_);
  views::InstallCircleHighlightPathGenerator(header_toggle_button_);
  header_toggle_button_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  views::FocusRing::Install(header_toggle_button_);
  views::FocusRing::Get(header_toggle_button_)
      ->SetHasFocusPredicate(base::BindRepeating(
          [](const OmniboxHeaderView* header, const View* view) {
            return view->GetVisible() &&
                   header->popup_view_->model()->GetPopupSelection() ==
                       header->GetHeaderSelection();
          },
          base::Unretained(this)));
  views::FocusRing::Get(header_toggle_button_)
      ->SetOutsetFocusRingDisabled(true);

  UpdateExpandedCollapsedAccessibleState();
}

void OmniboxHeaderView::SetHeader(const std::u16string& header_text,
                                  bool is_suggestion_group_hidden) {
  header_text_ = header_text;

  // TODO(tommycli): Our current design calls for uppercase text here, but
  // it seems like an open question what should happen for non-Latin locales.
  // Moreover, it seems unusual to do case conversion in Views in general.
  std::u16string header_str = header_text_;
    header_str = base::i18n::ToUpper(header_str);
  header_label_->SetText(header_str);
  header_toggle_button_->SetToggled(is_suggestion_group_hidden);
}

gfx::Insets OmniboxHeaderView::GetInsets() const {
  // Makes the header height roughly the same as the single-line row height.
  const int vertical = 8;

  // Aligns the header text with the icons of ordinary matches. The assumed
  // small icon width here is lame, but necessary, since it's not explicitly
  // defined anywhere else in the code.
  constexpr int assumed_match_cell_icon_width = 16;
  constexpr int left_inset = OmniboxMatchCellView::kMarginLeft +
                             (OmniboxMatchCellView::kImageBoundsWidth -
                              assumed_match_cell_icon_width) /
                                 2;

  return gfx::Insets::TLBR(vertical, left_inset, vertical,
                           OmniboxMatchCellView::kMarginRight);
}

bool OmniboxHeaderView::OnMousePressed(const ui::MouseEvent& event) {
  // Needed to receive the OnMouseReleased event.
  return true;
}

void OmniboxHeaderView::OnMouseReleased(const ui::MouseEvent& event) {
  popup_view_->model()->OpenSelection(GetHeaderSelection(), event.time_stamp());
}

void OmniboxHeaderView::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateUI();
}

void OmniboxHeaderView::OnMouseExited(const ui::MouseEvent& event) {
  UpdateUI();
}

void OmniboxHeaderView::OnThemeChanged() {
  views::View::OnThemeChanged();

  // When the theme is updated, also refresh the hover-specific UI, which is
  // all of the UI.
  UpdateUI();
}

void OmniboxHeaderView::UpdateUI() {
  OmniboxPartState part_state = OmniboxPartState::NORMAL;
  if (popup_view_->model()->GetPopupSelection() == GetHeaderSelection()) {
    part_state = OmniboxPartState::SELECTED;
  } else if (IsMouseHovered()) {
    part_state = OmniboxPartState::HOVERED;
  }

  const auto* const color_provider = GetColorProvider();
  const SkColor text_color =
      color_provider->GetColor((part_state == OmniboxPartState::SELECTED)
                                   ? kColorOmniboxResultsTextDimmedSelected
                                   : kColorOmniboxResultsTextDimmed);
  header_label_->SetEnabledColor(text_color);

  const SkColor icon_color =
      color_provider->GetColor((part_state == OmniboxPartState::SELECTED)
                                   ? kColorOmniboxResultsIconSelected
                                   : kColorOmniboxResultsIcon);
  views::InkDrop::Get(header_toggle_button_)->SetBaseColor(icon_color);

  int dip_size = GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
  const gfx::ImageSkia arrow_down = gfx::CreateVectorIcon(
      omnibox::kArrowDownChromeRefreshIcon, dip_size, icon_color);
  const ui::ImageModel arrow_up = ui::ImageModel::FromVectorIcon(
      omnibox::kArrowUpChromeRefreshIcon, icon_color, dip_size);

  // The "untoggled" button state corresponds with the group being shown.
  // The button's action is therefore to Hide the group, when clicked.
  header_toggle_button_->SetImageModel(views::Button::STATE_NORMAL, arrow_up);
  header_toggle_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_HEADER_HIDE_SUGGESTIONS_BUTTON));
  header_toggle_button_->GetViewAccessibility().SetName(
      l10n_util::GetStringFUTF16(IDS_ACC_HEADER_HIDE_SUGGESTIONS_BUTTON,
                                 header_text_));

  // The "toggled" button state corresponds with the group being hidden.
  // The button's action is therefore to Show the group, when clicked.
  header_toggle_button_->SetToggledImageModel(
      views::Button::STATE_NORMAL, ui::ImageModel::FromImageSkia(arrow_down));
  header_toggle_button_->SetToggledTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_HEADER_SHOW_SUGGESTIONS_BUTTON));
  header_toggle_button_->SetToggledAccessibleName(l10n_util::GetStringFUTF16(
      IDS_ACC_HEADER_SHOW_SUGGESTIONS_BUTTON, header_text_));

  views::FocusRing::Get(header_toggle_button_)->SchedulePaint();
}

void OmniboxHeaderView::HeaderToggleButtonPressed() {
  popup_view_->model()->OpenSelection(GetHeaderSelection(), base::TimeTicks());
  // The PrefChangeRegistrar will update the actual button toggle state.
}

void OmniboxHeaderView::SetSuggestionGroupVisibility(
    bool suggestion_group_hidden) {
  suggestion_group_hidden_ = suggestion_group_hidden;

  UpdateExpandedCollapsedAccessibleState();

  // Because this view doesn't have true focus (it stays on the textfield),
  // we also need to manually announce state changes.
  GetViewAccessibility().AnnounceText(l10n_util::GetStringFUTF16(
      suggestion_group_hidden_ ? IDS_ACC_HEADER_SECTION_HIDDEN
                               : IDS_ACC_HEADER_SECTION_SHOWN,
      header_text_));

  header_toggle_button_->SetToggled(suggestion_group_hidden_);
}

// Convenience method to get the OmniboxPopupSelection for this view.
OmniboxPopupSelection OmniboxHeaderView::GetHeaderSelection() const {
  return OmniboxPopupSelection(model_index_,
                               OmniboxPopupSelection::FOCUSED_BUTTON_HEADER);
}

void OmniboxHeaderView::UpdateExpandedCollapsedAccessibleState() const {
  // Hidden OmniboxHeaderView instances are not associated with any group ID, so
  // they are neither collapsed or expanded.
  if (!GetVisible()) {
    GetViewAccessibility().RemoveExpandCollapseState();
    return;
  }

  if (suggestion_group_hidden_) {
    GetViewAccessibility().SetIsCollapsed();
  } else {
    GetViewAccessibility().SetIsExpanded();
  }
}

BEGIN_METADATA(OmniboxHeaderView)
ADD_READONLY_PROPERTY_METADATA(OmniboxPopupSelection, HeaderSelection)
END_METADATA
