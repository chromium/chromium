// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_row_view.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/omnibox/omnibox_header_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/type_conversion.h"
#include "ui/views/style/typography.h"

DEFINE_ENUM_CONVERTERS(OmniboxPopupSelection::LineState,
                       {OmniboxPopupSelection::FOCUSED_BUTTON_HEADER,
                        u"FOCUSED_BUTTON_HEADER"},
                       {OmniboxPopupSelection::NORMAL, u"NORMAL"},
                       {OmniboxPopupSelection::KEYWORD_MODE, u"KEYWORD_MODE"},
                       {OmniboxPopupSelection::FOCUSED_BUTTON_ACTION,
                        u"FOCUSED_BUTTON_ACTION"},
                       {OmniboxPopupSelection::FOCUSED_BUTTON_THUMBS_UP,
                        u"FOCUSED_BUTTON_THUMBS_UP"},
                       {OmniboxPopupSelection::FOCUSED_BUTTON_THUMBS_DOWN,
                        u"FOCUSED_BUTTON_THUMBS_DOWN"},
                       {OmniboxPopupSelection::FOCUSED_BUTTON_REMOVE_SUGGESTION,
                        u"FOCUSED_BUTTON_REMOVE_SUGGESTION"})

template <>
struct ui::metadata::TypeConverter<OmniboxPopupSelection>
    : public ui::metadata::BaseTypeConverter<true> {
  static std::u16string ToString(
      ui::metadata::ArgType<OmniboxPopupSelection> source_value);
  static std::optional<OmniboxPopupSelection> FromString(
      const std::u16string& source_value);
  static ui::metadata::ValidStrings GetValidStrings() { return {}; }
};

// static
std::u16string ui::metadata::TypeConverter<OmniboxPopupSelection>::ToString(
    ui::metadata::ArgType<OmniboxPopupSelection> source_value) {
  return u"{" + base::NumberToString16(source_value.line) + u"," +
         TypeConverter<OmniboxPopupSelection::LineState>::ToString(
             source_value.state) +
         u"}";
}

// static
std::optional<OmniboxPopupSelection> ui::metadata::TypeConverter<
    OmniboxPopupSelection>::FromString(const std::u16string& source_value) {
  const auto values = base::SplitString(
      source_value, u"{,}", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (values.size() != 2) {
    return std::nullopt;
  }
  // TODO(pkasting): This should be size_t, but for some reason that won't link
  // on Mac.
  const std::optional<uint32_t> line =
      TypeConverter<uint32_t>::FromString(values[0]);
  const std::optional<OmniboxPopupSelection::LineState> state =
      TypeConverter<OmniboxPopupSelection::LineState>::FromString(values[1]);
  return (line.has_value() && state.has_value())
             ? std::make_optional<OmniboxPopupSelection>(line.value(),
                                                         state.value())
             : std::nullopt;
}

OmniboxRowView::OmniboxRowView(size_t line, OmniboxPopupViewViews* popup_view)
    : line_(line), popup_view_(popup_view) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  result_view_ =
      AddChildView(std::make_unique<OmniboxResultView>(popup_view, line));
}

void OmniboxRowView::ShowHeader(const std::u16string& header_text,
                                bool suggestion_group_hidden) {
  // Create the header (at index 0) if it doesn't exist.
  if (header_view_ == nullptr) {
    header_view_ = AddChildViewAt(
        std::make_unique<OmniboxHeaderView>(popup_view_, line_), 0);
  }

  header_view_->SetHeader(header_text, suggestion_group_hidden);
  header_view_->SetVisible(true);
}

void OmniboxRowView::HideHeader() {
  if (header_view_) {
    header_view_->SetVisible(false);
  }
}

void OmniboxRowView::OnSelectionStateChanged() {
  result_view_->OnSelectionStateChanged();
  if (header_view_ && header_view_->GetVisible()) {
    header_view_->UpdateUI();
  }
}

views::View* OmniboxRowView::GetActiveAuxiliaryButtonForAccessibility() const {
  DCHECK(popup_view_->model()->GetPopupSelection().IsButtonFocused());
  if (popup_view_->model()->GetPopupSelection().state ==
      OmniboxPopupSelection::FOCUSED_BUTTON_HEADER) {
    return header_view_->header_toggle_button();
  }

  return result_view_->GetActiveAuxiliaryButtonForAccessibility();
}

gfx::Insets OmniboxRowView::GetInsets() const {
  if (result_view_->GetThemeState() == OmniboxPartState::IPH) {
    int LRInsets = OmniboxMatchCellView::kIphOffset;
    return gfx::Insets::TLBR(8, LRInsets, 8, LRInsets);
  }

  return gfx::Insets::TLBR(0, 0, 0, 16);
}

BEGIN_METADATA(OmniboxRowView)
END_METADATA
