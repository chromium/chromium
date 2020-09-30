// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_row_view.h"

#include "base/bind.h"
#include "base/i18n/case_conversion.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_mouse_enter_exit_handler.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"

class OmniboxRowView::HeaderView : public views::View {
 public:
  explicit HeaderView(OmniboxRowView* row_view)
      : row_view_(row_view),
        // Using base::Unretained is correct here. 'this' outlives the callback.
        mouse_enter_exit_handler_(base::BindRepeating(&HeaderView::UpdateUI,
                                                      base::Unretained(this))) {
    views::BoxLayout* layout =
        SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
    // This is the designer-provided spacing that matches the NTP Realbox.
    layout->set_between_child_spacing(8);

    header_label_ = AddChildView(std::make_unique<views::Label>());
    header_label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

    const gfx::FontList& font =
        views::style::GetFont(CONTEXT_OMNIBOX_PRIMARY,
                              views::style::STYLE_PRIMARY)
            .DeriveWithWeight(gfx::Font::Weight::MEDIUM);
    header_label_->SetFontList(font);

    header_toggle_button_ =
        AddChildView(views::CreateVectorToggleImageButton(base::BindRepeating(
            &HeaderView::HeaderToggleButtonPressed, base::Unretained(this))));
    mouse_enter_exit_handler_.ObserveMouseEnterExitOn(header_toggle_button_);
    views::InstallCircleHighlightPathGenerator(header_toggle_button_);

    header_toggle_button_focus_ring_ =
        views::FocusRing::Install(header_toggle_button_);
    header_toggle_button_focus_ring_->SetHasFocusPredicate([&](View* view) {
      return view->GetVisible() &&
             row_view_->popup_model_->selection() == HeaderSelection();
    });

    if (row_view_->pref_service_) {
      pref_change_registrar_.Init(row_view_->pref_service_);
      // Unretained is appropriate here. 'this' will outlive the registrar.
      pref_change_registrar_.Add(omnibox::kSuggestionGroupVisibility,
                                 base::BindRepeating(&HeaderView::OnPrefChanged,
                                                     base::Unretained(this)));
    }
  }

  void SetHeader(int suggestion_group_id, const base::string16& header_text) {
    suggestion_group_id_ = suggestion_group_id;
    header_text_ = header_text;

    // TODO(tommycli): Our current design calls for uppercase text here, but
    // it seems like an open question what should happen for non-Latin locales.
    // Moreover, it seems unusual to do case conversion in Views in general.
    header_label_->SetText(base::i18n::ToUpper(header_text_));

    if (row_view_->pref_service_) {
      suggestion_group_hidden_ =
          row_view_->popup_model_->result().IsSuggestionGroupIdHidden(
              row_view_->pref_service_, suggestion_group_id_);

      header_toggle_button_->SetToggled(suggestion_group_hidden_);
    }
  }

  // views::View:
  gfx::Insets GetInsets() const override {
    // Makes the header height roughly the same as the single-line row height.
    constexpr int vertical = 6;

    // Aligns the header text with the icons of ordinary matches. The assumed
    // small icon width here is lame, but necessary, since it's not explicitly
    // defined anywhere else in the code.
    constexpr int assumed_match_cell_icon_width = 16;
    constexpr int left_inset = OmniboxMatchCellView::kMarginLeft +
                               (OmniboxMatchCellView::kImageBoundsWidth -
                                assumed_match_cell_icon_width) /
                                   2;

    return gfx::Insets(vertical, left_inset, vertical,
                       OmniboxMatchCellView::kMarginRight);
  }
  bool OnMousePressed(const ui::MouseEvent& event) override {
    // Needed to receive the OnMouseReleased event.
    return true;
  }
  void OnMouseReleased(const ui::MouseEvent& event) override {
    row_view_->popup_model_->TriggerSelectionAction(HeaderSelection());
  }
  void OnMouseEntered(const ui::MouseEvent& event) override { UpdateUI(); }
  void OnMouseExited(const ui::MouseEvent& event) override { UpdateUI(); }
  void OnThemeChanged() override {
    views::View::OnThemeChanged();

    // When the theme is updated, also refresh the hover-specific UI, which is
    // all of the UI.
    UpdateUI();
  }
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    // Hidden HeaderView instances are not associated with any group ID, so they
    // are neither collapsed or expanded.s
    if (!GetVisible())
      return;

    node_data->AddState(suggestion_group_hidden_ ? ax::mojom::State::kCollapsed
                                                 : ax::mojom::State::kExpanded);
  }

  // Updates the UI state for the new hover or selection state.
  void UpdateUI() {
    OmniboxPartState part_state = OmniboxPartState::NORMAL;
    if (row_view_->popup_model_->selection() == HeaderSelection()) {
      part_state = OmniboxPartState::SELECTED;
    } else if (IsMouseHovered()) {
      part_state = OmniboxPartState::HOVERED;
    }

    SkColor text_color = GetOmniboxColor(
        GetThemeProvider(), OmniboxPart::RESULTS_TEXT_DIMMED, part_state);
    header_label_->SetEnabledColor(text_color);

    SkColor icon_color = GetOmniboxColor(GetThemeProvider(),
                                         OmniboxPart::RESULTS_ICON, part_state);
    header_toggle_button_->SetInkDropBaseColor(icon_color);

    int dip_size = GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
    const gfx::ImageSkia arrow_down =
        gfx::CreateVectorIcon(omnibox::kChevronIcon, dip_size, icon_color);
    const gfx::ImageSkia arrow_up =
        gfx::ImageSkiaOperations::CreateRotatedImage(
            arrow_down, SkBitmapOperations::ROTATION_180_CW);

    // The "untoggled" button state corresponds with the group being shown.
    // The button's action is therefore to Hide the group, when clicked.
    header_toggle_button_->SetImage(views::Button::STATE_NORMAL, arrow_up);
    header_toggle_button_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_TOOLTIP_HEADER_HIDE_SUGGESTIONS_BUTTON));
    header_toggle_button_->SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_ACC_HEADER_HIDE_SUGGESTIONS_BUTTON, header_text_));

    // The "toggled" button state corresponds with the group being hidden.
    // The button's action is therefore to Show the group, when clicked.
    header_toggle_button_->SetToggledImage(views::Button::STATE_NORMAL,
                                           &arrow_down);
    header_toggle_button_->SetToggledTooltipText(
        l10n_util::GetStringUTF16(IDS_TOOLTIP_HEADER_SHOW_SUGGESTIONS_BUTTON));
    header_toggle_button_->SetToggledAccessibleName(l10n_util::GetStringFUTF16(
        IDS_ACC_HEADER_SHOW_SUGGESTIONS_BUTTON, header_text_));

    header_toggle_button_focus_ring_->SchedulePaint();

    // It's a little hokey that we're stealing the logic for the background
    // color from OmniboxResultView. If we start doing this is more than just
    // one place, we should introduce a more elegant abstraction here.
    SetBackground(OmniboxResultView::GetPopupCellBackground(this, part_state));
  }

  views::Button* header_toggle_button() const { return header_toggle_button_; }

 private:
  void HeaderToggleButtonPressed() {
    row_view_->popup_model_->TriggerSelectionAction(HeaderSelection());
    // The PrefChangeRegistrar will update the actual button toggle state.
  }

  // Updates the hide button's toggle state.
  void OnPrefChanged() {
    DCHECK(row_view_->pref_service_);
    bool was_hidden = suggestion_group_hidden_;
    suggestion_group_hidden_ =
        row_view_->popup_model_->result().IsSuggestionGroupIdHidden(
            row_view_->pref_service_, suggestion_group_id_);

    if (was_hidden != suggestion_group_hidden_) {
      NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);

      // Because this view doesn't have true focus (it stays on the textfield),
      // we also need to manually announce state changes.
      GetViewAccessibility().AnnounceText(l10n_util::GetStringFUTF16(
          suggestion_group_hidden_ ? IDS_ACC_HEADER_SECTION_HIDDEN
                                   : IDS_ACC_HEADER_SECTION_SHOWN,
          header_text_));
    }

    header_toggle_button_->SetToggled(suggestion_group_hidden_);
  }

  // Convenience method to get the OmniboxPopupModel::Selection for this view.
  OmniboxPopupModel::Selection HeaderSelection() {
    return OmniboxPopupModel::Selection(
        row_view_->line_, OmniboxPopupModel::FOCUSED_BUTTON_HEADER);
  }

  // Non-owning pointer our parent row view. We access a lot of private members
  // of our outer class. This lets us save quite a bit of state duplication.
  OmniboxRowView* const row_view_;

  // The Label containing the header text. This is never nullptr.
  views::Label* header_label_;

  // The button used to toggle hiding suggestions with this header.
  views::ToggleImageButton* header_toggle_button_;
  views::FocusRing* header_toggle_button_focus_ring_ = nullptr;

  // The group ID associated with this header.
  int suggestion_group_id_ = 0;

  // The unmodified header text for this header.
  base::string16 header_text_;

  // Stores whether or not the group was hidden. This is used to fire correct
  // accessibility change events.
  bool suggestion_group_hidden_ = false;

  // A pref change registrar for toggling the toggle button's state. This is
  // needed because the preference state can change through multiple UIs.
  PrefChangeRegistrar pref_change_registrar_;

  // Keeps track of mouse-enter and mouse-exit events of child Views.
  OmniboxMouseEnterExitHandler mouse_enter_exit_handler_;
};

OmniboxRowView::OmniboxRowView(size_t line,
                               OmniboxPopupModel* popup_model,
                               std::unique_ptr<OmniboxResultView> result_view,
                               PrefService* pref_service)
    : line_(line), popup_model_(popup_model), pref_service_(pref_service) {
  DCHECK(result_view);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  result_view_ = AddChildView(std::move(result_view));
}

void OmniboxRowView::ShowHeader(int suggestion_group_id,
                                const base::string16& header_text) {
  // Create the header (at index 0) if it doesn't exist.
  if (header_view_ == nullptr)
    header_view_ = AddChildViewAt(std::make_unique<HeaderView>(this), 0);

  header_view_->SetHeader(suggestion_group_id, header_text);
  header_view_->SetVisible(true);
}

void OmniboxRowView::HideHeader() {
  if (header_view_)
    header_view_->SetVisible(false);
}

void OmniboxRowView::OnSelectionStateChanged() {
  result_view_->OnSelectionStateChanged();
  if (header_view_ && header_view_->GetVisible())
    header_view_->UpdateUI();
}

views::View* OmniboxRowView::GetActiveAuxiliaryButtonForAccessibility() const {
  DCHECK(popup_model_->selection().IsButtonFocused());
  if (popup_model_->selected_line_state() ==
      OmniboxPopupModel::FOCUSED_BUTTON_HEADER) {
    return header_view_->header_toggle_button();
  }

  // TODO(tommycli): This needs to be updated to properly support the
  // suggestion button row. The name would need to be updated too.
  return result_view_->GetSecondaryButton();
}

gfx::Insets OmniboxRowView::GetInsets() const {
  // A visible header means this is the start of a new section. Give the section
  // that just ended an extra 4dp of padding. https://crbug.com/1076646
  if (line_ != 0 && header_view_ && header_view_->GetVisible())
    return gfx::Insets(4, 0, 0, 0);

  return gfx::Insets();
}
