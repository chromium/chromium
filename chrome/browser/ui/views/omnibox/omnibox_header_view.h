// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_HEADER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_HEADER_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/omnibox/omnibox_mouse_enter_exit_handler.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"

class OmniboxPopupViewViews;
struct OmniboxPopupSelection;

namespace gfx {
class Insets;
}

namespace views {
class Label;
}  // namespace views

namespace ui {
class MouseEvent;
}  // namespace ui

class OmniboxHeaderView : public views::View {
  METADATA_HEADER(OmniboxHeaderView, views::View)

 public:
  OmniboxHeaderView(OmniboxPopupViewViews* popup_view, size_t model_index);

  void SetHeader(const std::u16string& header_text,
                 bool is_suggestion_group_hidden);

  // views::View:
  gfx::Insets GetInsets() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnThemeChanged() override;

  // Updates the UI state for the new hover or selection state.
  void UpdateUI();

  views::Button* header_toggle_button() const { return header_toggle_button_; }

  // Updates the hide button's toggle state.
  void SetSuggestionGroupVisibility(bool suggestion_group_hidden);

 private:
  void HeaderToggleButtonPressed();

  // Convenience method to get the OmniboxPopupSelection for this view.
  OmniboxPopupSelection GetHeaderSelection() const;

  void UpdateExpandedCollapsedAccessibleState() const;

  // The parent view.
  const raw_ptr<OmniboxPopupViewViews> popup_view_;

  // This header's associated result model index.
  size_t model_index_;

  // The Label containing the header text. This is never nullptr.
  raw_ptr<views::Label> header_label_;

  // The button used to toggle hiding suggestions with this header.
  raw_ptr<views::ToggleImageButton> header_toggle_button_;

  // The unmodified header text for this header.
  std::u16string header_text_;

  // Stores whether or not the group was hidden. This is used to fire correct
  // accessibility change events.
  bool suggestion_group_hidden_ = false;

  // Keeps track of mouse-enter and mouse-exit events of child Views.
  OmniboxMouseEnterExitHandler mouse_enter_exit_handler_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_HEADER_VIEW_H_
