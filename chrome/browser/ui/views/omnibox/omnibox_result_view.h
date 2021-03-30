// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_RESULT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_RESULT_VIEW_H_

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/omnibox/omnibox_mouse_enter_exit_handler.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class OmniboxMatchCellView;
class OmniboxPopupContentsView;
class OmniboxSuggestionButtonRowView;
class OmniboxTabSwitchButton;
class OmniboxResultSelectionIndicator;
enum class OmniboxPart;
enum class OmniboxPartState;

namespace gfx {
class Image;
}

namespace views {
class Button;
class FocusRing;
class ImageButton;
}  // namespace views

class OmniboxResultView : public views::View,
                          public views::AnimationDelegateViews {
 public:
  METADATA_HEADER(OmniboxResultView);
  OmniboxResultView(OmniboxPopupContentsView* popup_contents_view,
                    size_t model_index);
  OmniboxResultView(const OmniboxResultView&) = delete;
  OmniboxResultView& operator=(const OmniboxResultView&) = delete;
  ~OmniboxResultView() override;

  // Static method to share logic about how to set backgrounds of popup cells.
  static std::unique_ptr<views::Background> GetPopupCellBackground(
      views::View* view,
      OmniboxPartState part_state);

  // Helper to get the color for |part| using the current state.
  SkColor GetColor(OmniboxPart part) const;

  // Updates the match used to paint the contents of this result view. We copy
  // the match so that we can continue to paint the last result even after the
  // model has changed.
  void SetMatch(const AutocompleteMatch& match);

  // Sets the visibility of the keyword mode slide animation.
  void ShowKeywordSlideAnimation(bool show_keyword);

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

  void ButtonPressed(OmniboxPopupModel::LineState state,
                     const ui::Event& event);

  // Helper to emit accessibility events (may only emit if conditions are met).
  void EmitTextChangedAccessiblityEvent();

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnThemeChanged() override;

 private:
  gfx::Image GetIcon() const;

  // Updates the highlight state of the row, as well as conditionally shows
  // controls that are only visible on row hover.
  void UpdateHoverState();

  // This returns true if the match has a matching tab and will use a
  // switch-to-tab button inline in Result View. It returns false, for
  // example, when the switch button is not shown because a keyword match is
  // taking precedence or when Suggestion Button Row is enabled, as the
  // Switch-to-tab button will appear in the button row.
  bool ShouldShowTabMatchButtonInline();

  // Sets the visibility of the |remove_suggestion_button_| based on the current
  // state.
  void UpdateRemoveSuggestionVisibility();

  // Sets the widths of the suggestion and keyword and calls Layout().
  void SetWidths();

  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // The parent view.
  OmniboxPopupContentsView* const popup_contents_view_;

  // This result's model index.
  size_t model_index_;

  // The data this class is built to display (the "Omnibox Result").
  AutocompleteMatch match_;

  // Accessible name (enables to emit certain events).
  std::u16string accessible_name_;

  // For sliding in the keyword search.
  std::unique_ptr<gfx::SlideAnimation> keyword_slide_animation_;

  // Container for the first row (for everything expect |button_row_|).
  views::View* suggestion_container_;

  // Weak pointers for easy reference.
  OmniboxMatchCellView* suggestion_view_;  // The leading (or left) view.
  OmniboxMatchCellView* keyword_view_;     // The trailing (or right) view.
  OmniboxTabSwitchButton* suggestion_tab_switch_button_;

  // The blue bar used to indicate selection. This is currently only used if
  // omnibox-refined-focus-state flag is enabled.
  OmniboxResultSelectionIndicator* selection_indicator_ = nullptr;

  // The "X" button at the end of the match cell, used to remove suggestions.
  views::ImageButton* remove_suggestion_button_;
  views::FocusRing* remove_suggestion_focus_ring_ = nullptr;

  // The row of buttons, only assigned and used if OmniboxSuggestionButtonRow
  // feature is enabled. It is owned by the base view, not this raw pointer.
  OmniboxSuggestionButtonRowView* button_row_ = nullptr;

  // Keeps track of mouse-enter and mouse-exit events of child Views.
  OmniboxMouseEnterExitHandler mouse_enter_exit_handler_;

  base::WeakPtrFactory<OmniboxResultView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_RESULT_VIEW_H_
