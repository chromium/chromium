// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_NOTICE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_NOTICE_VIEW_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/autofill/popup/popup_interactive_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "components/input/native_web_keyboard_event.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class MdTextButton;
class Link;
class StyledLabel;
}  // namespace views

namespace autofill {

class AutofillPopupController;

// Outcomes of interaction with an Autofill popup notice.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PopupNoticeInteractions)
enum class PopupNoticeInteractions {
  kShown = 0,
  kAcknowledged = 1,
  kDismissed = 2,
  kLinkButtonClicked = 3,
  kMaxValue = kLinkButtonClicked,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/personal_context/enums.xml:PopupNoticeInteractions)

// The view that displays an informational notice (such as for Personal Context
// or Ambient Autofill) at the bottom of the Autofill popup.
class PopupNoticeView : public PopupInteractiveRowView {
  METADATA_HEADER(PopupNoticeView, PopupInteractiveRowView)

 public:
  PopupNoticeView(
      PopupRowView::AccessibilitySelectionDelegate& a11y_selection_delegate,
      base::RepeatingCallback<void(const std::u16string&, bool)>
          announce_callback,
      base::WeakPtr<AutofillPopupController> controller,
      int line_number,
      std::u16string_view title_text,
      std::u16string_view subtitle_text,
      std::u16string_view link_text,
      std::u16string_view accept_button_text,
      base::RepeatingClosure on_link_clicked,
      std::string_view notice_interaction_histogram_name);

  // PopupInteractiveRowView:
  std::optional<CellType> GetSelectedCell() const override;
  // When entering the notice view row, sets the focus on the link.
  // When leaving it, removes the focus from any element currently focused.
  void SetSelectedCell(std::optional<CellType> cell) override;
  bool HandleKeyPressEvent(const input::NativeWebKeyboardEvent& event) override;
  bool IsSelectable() const override;

  PopupNoticeView(const PopupNoticeView&) = delete;
  PopupNoticeView& operator=(const PopupNoticeView&) = delete;
  ~PopupNoticeView() override;

  views::StyledLabel* description_for_testing() const { return description_; }
  views::MdTextButton* accept_button_for_testing() const {
    return accept_button_;
  }
  bool is_link_focused_for_testing() const { return is_link_focused_; }
  bool is_accept_button_focused_for_testing() const {
    return is_accept_button_focused_;
  }

 private:
  // Marks the notice as acknowledged and removes it from the parent view.
  void OnAcceptButtonClicked();

  // Invokes the link clicked closure (such as opening Chrome settings) and
  // records the link click metric.
  void OnLinkClicked();

  // Set the navigation focus on the link.
  void FocusLink();

  // Remove the navigation focus from the link.
  void UnfocusLink();

  // Applies or clears the focus border styling on all link fragments.
  void UpdateLinkBorders(bool focused);

  // Set the navigation focus on the accept button.
  void FocusAcceptButton();

  // Remove the navigation focus from the accept button.
  void UnfocusAcceptButton();

  // Returns the link element inside of `description_`.
  views::Link* GetLink() const;

  // views::View:
  // Configures child views (such as the link) that are lazily created during
  // the layout pass of `description_`.
  void Layout(views::View::PassKey pass_key) override;
  // Returns a minimum size with a width of `kMinimumWidth` to keep the notice
  // content readable.
  gfx::Size GetMinimumSize() const override;
  // Calculates the preferred size to support height-for-width calculations.
  // Passing the target width constraint to the base class allows the wrapping
  // label child to calculate its preferred height based on text wrapping.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_bounds) const override;

  // The description text inside of the notice element.
  raw_ptr<views::StyledLabel> description_ = nullptr;

  // The accept button users click to acknowledge the notice.
  raw_ptr<views::MdTextButton> accept_button_ = nullptr;

  // True if the navigation focus is currently on the link.
  bool is_link_focused_ = false;

  // True if the navigation focus is currently on the accept button.
  bool is_accept_button_focused_ = false;

  // The controller for the popup this notice is part of.
  const base::WeakPtr<AutofillPopupController> controller_;

  // The position of this notice in the vertical list of suggestions.
  const int line_number_;

  const base::RepeatingClosure on_link_clicked_;
  const std::string notice_interaction_histogram_name_;
  const base::RepeatingCallback<void(const std::u16string&, bool)>
      announce_callback_;
  const raw_ref<PopupRowView::AccessibilitySelectionDelegate>
      a11y_selection_delegate_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_NOTICE_VIEW_H_
