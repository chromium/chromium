// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_RESULT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_RESULT_VIEW_H_

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/omnibox/omnibox_mouse_enter_exit_handler.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

class OmniboxMatchCellView;
class OmniboxPopupViewViews;
class OmniboxResultSelectionIndicator;
class OmniboxSuggestionButtonRowView;
enum class OmniboxPart;
enum class OmniboxPartState;

namespace gfx {
class Image;
}

namespace views {
class Button;
class ImageButton;
}  // namespace views

class OmniboxResultView : public views::View {
  METADATA_HEADER(OmniboxResultView, views::View)

 public:
  OmniboxResultView(OmniboxPopupViewViews* popup_view, size_t model_index);
  OmniboxResultView(const OmniboxResultView&) = delete;
  OmniboxResultView& operator=(const OmniboxResultView&) = delete;
  ~OmniboxResultView() override;

  // Static method to share logic about how to set backgrounds of popup cells.
  static std::unique_ptr<views::Background> GetPopupCellBackground(
      views::View* view,
      OmniboxPartState part_state);

  // Updates the match used to paint the contents of this result view. We copy
  // the match so that we can continue to paint the last result even after the
  // model has changed.
  void SetMatch(const AutocompleteMatch& match);

  // Applies the current theme to the current text and widget colors.
  // Also refreshes the icons which may need to be re-colored as well.
  void ApplyThemeAndRefreshIcons(bool force_reapply_styles = false);

  // Invoked when this result view has been selected or unselected.
  void OnSelectionStateChanged();

  // Whether this result view should be considered 'selected'. This returns
  // false if this line's header is selected (instead of the match itself).
  bool GetMatchSelected() const;

  // Returns the focused button or nullptr if none exists for this suggestion.
  views::Button* GetActiveAuxiliaryButtonForAccessibility();

  OmniboxPartState GetThemeState() const;

  // Notification that the match icon has changed and schedules a repaint.
  void OnMatchIconUpdated();

  // Stores the image in a local data member and schedules a repaint.
  void SetRichSuggestionImage(const gfx::ImageSkia& image);

  void ButtonPressed(OmniboxPopupSelection::LineState state,
                     const ui::Event& event);

  // Helper to emit accessibility events (may only emit if conditions are met).
  void EmitTextChangedAccessiblityEvent();

  void UpdateAccessibilityProperties();

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnThemeChanged() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupViewViewsTest, DeleteSuggestion);

  void OpenIphLink();

  gfx::Image GetIcon() const;

  // Updates the highlight state of the row, as well as conditionally shows
  // controls that are only visible on row hover.
  void UpdateHoverState();

  // Sets the visibility of the |thumbs_up_button_| and |thumbs_down_button_|
  // based on the current state.
  void UpdateFeedbackButtonsVisibility();

  // Sets the visibility of the |remove_suggestion_button_| based on the current
  // state.
  void UpdateRemoveSuggestionVisibility();

  // Updates the 'selected' state of the view as applicable based on whether or
  // not the view is selected.
  void UpdateAccessibilitySelectedState();

  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // The parent view.
  const raw_ptr<OmniboxPopupViewViews> popup_view_;

  // This result's model index.
  const size_t model_index_;

  // The data this class is built to display (the "Omnibox Result").
  AutocompleteMatch match_;

  // Accessible name (enables to emit certain events).
  std::u16string accessible_name_;

  // Weak pointers for easy reference.
  raw_ptr<OmniboxMatchCellView>
      suggestion_view_;  // The leading (or left) view.

  // The blue bar used to indicate selection.
  raw_ptr<OmniboxResultSelectionIndicator> selection_indicator_ = nullptr;

  // The thumbs up button used to submit feedback for suggestions.
  raw_ptr<views::ImageButton> thumbs_up_button_;

  // The thumbs down button used to submit feedback for suggestions.
  raw_ptr<views::ImageButton> thumbs_down_button_;

  // The "X" button at the end of the match cell, used to remove suggestions.
  raw_ptr<views::ImageButton> remove_suggestion_button_;

  // The row of buttons that appears when actions such as tab switch or Pedals
  // are on the suggestion. It is owned by the base view, not this raw pointer.
  raw_ptr<OmniboxSuggestionButtonRowView> button_row_ = nullptr;

  // Keeps track of mouse-enter and mouse-exit events of child Views.
  OmniboxMouseEnterExitHandler mouse_enter_exit_handler_;

  base::WeakPtrFactory<OmniboxResultView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_RESULT_VIEW_H_
