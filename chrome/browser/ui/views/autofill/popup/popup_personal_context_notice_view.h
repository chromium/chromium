// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_PERSONAL_CONTEXT_NOTICE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_PERSONAL_CONTEXT_NOTICE_VIEW_H_

#include <optional>

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
}

namespace autofill {

class AutofillPopupController;

// Outcomes of interaction with the Ambient Autofill notice.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PersonalContextAmbientAutofillNoticeInteractions)
enum class PersonalContextAmbientAutofillNoticeInteractions {
  kShown = 0,
  kAcknowledged = 1,
  kDismissed = 2,
  kManageSettingsButtonClicked = 3,
  kMaxValue = kManageSettingsButtonClicked,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/personal_context/enums.xml:PersonalContextAmbientAutofillNoticeInteractions)

// Outcomes of interaction with the AtMemory notice.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PersonalContextAtMemoryNoticeInteractions)
enum class PersonalContextAtMemoryNoticeInteractions {
  kShown = 0,
  kAcknowledged = 1,
  kMaxValue = kAcknowledged,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/personal_context/enums.xml:PersonalContextAtMemoryNoticeInteractions)

// The view that displays the "Personal context" notice.
// This notice is shown at the bottom of the Autofill popup to inform the
// user that personal context is enabled.
class PopupPersonalContextNoticeView : public PopupInteractiveRowView {
  METADATA_HEADER(PopupPersonalContextNoticeView, PopupInteractiveRowView)

 public:
  PopupPersonalContextNoticeView(
      PopupRowView::AccessibilitySelectionDelegate& a11y_selection_delegate,
      base::RepeatingCallback<void(const std::u16string&, bool)>
          announce_callback,
      base::WeakPtr<AutofillPopupController> controller,
      int line_number);

  // PopupInteractiveRowView:
  std::optional<CellType> GetSelectedCell() const override;
  // When entering the notice view row, sets the focus on the "Settings" link.
  // When leaving it, removes the focus from any element currently focused.
  void SetSelectedCell(std::optional<CellType> cell) override;
  bool HandleKeyPressEvent(const input::NativeWebKeyboardEvent& event) override;
  bool IsSelectable() const override;

  PopupPersonalContextNoticeView(const PopupPersonalContextNoticeView&) =
      delete;
  PopupPersonalContextNoticeView& operator=(
      const PopupPersonalContextNoticeView&) = delete;
  ~PopupPersonalContextNoticeView() override;

  views::StyledLabel* description_for_testing() const { return description_; }
  views::MdTextButton* got_it_button_for_testing() const {
    return got_it_button_;
  }
  bool is_link_focused_for_testing() const { return is_link_focused_; }
  bool is_button_focused_for_testing() const { return is_button_focused_; }

 private:
  // Marks the notice as acknowledged and removes it from the parent view.
  void OnGotItButtonClicked();

  // Opens personal context settings for autofill in Chrome settings.
  void OnSettingsLinkClicked();

  // Set the navigation focus on the "Settings" link.
  void FocusLink();

  // Remove the navigation focus from the "Settings" link.
  void UnfocusLink();

  // Applies or clears the focus border styling on all link fragments.
  void UpdateLinkBorders(bool focused);

  // Set the navigation focus on the "Got it" button.
  void FocusButton();

  // Remove the navigation focus from the "Got it" button.
  void UnfocusButton();

  // Returns the link element inside of `description_`.
  views::Link* GetSettingsLink() const;

  // views::View:
  // Configures child views (such as the settings link) that are lazily created
  // during the layout pass of `description_`.
  void Layout(views::View::PassKey pass_key) override;
  // Returns a minimum size with a width of `kMinimumWidth` to keep the notice
  // content readable.
  gfx::Size GetMinimumSize() const override;
  // Calculates the preferred size to support height-for-width calculations.
  // Passing the target width constraint to the base class allows the wrapping label child
  // to calculate its preferred height based on the text wrapping at this width.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_bounds) const override;

  // The description text inside of the notice element.
  raw_ptr<views::StyledLabel> description_ = nullptr;

  // The button users click to acknowledge the notice.
  raw_ptr<views::MdTextButton> got_it_button_ = nullptr;

  // True if the navigation focus is currently on the "Settings" link.
  bool is_link_focused_ = false;

  // True if the navigation focus is currently on the "Got it" button.
  bool is_button_focused_ = false;

  // The controller for the popup this notice is part of.
  const base::WeakPtr<AutofillPopupController> controller_;

  // The position of this notice in the vertical list of suggestions.
  const int line_number_;

  const base::RepeatingCallback<void(const std::u16string&, bool)>
      announce_callback_;

  const raw_ref<PopupRowView::AccessibilitySelectionDelegate>
      a11y_selection_delegate_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_PERSONAL_CONTEXT_NOTICE_VIEW_H_
