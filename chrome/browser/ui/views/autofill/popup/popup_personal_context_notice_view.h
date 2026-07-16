// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_PERSONAL_CONTEXT_NOTICE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_PERSONAL_CONTEXT_NOTICE_VIEW_H_

#include <optional>

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

// The view that displays the "Personal context" notice.
// This notice is shown at the bottom of the Autofill popup to inform the
// user that personal context is enabled.
class PopupPersonalContextNoticeView : public PopupInteractiveRowView {
  METADATA_HEADER(PopupPersonalContextNoticeView, PopupInteractiveRowView)

 public:
  PopupPersonalContextNoticeView(
      PopupRowView::AccessibilitySelectionDelegate& a11y_selection_delegate,
      base::WeakPtr<AutofillPopupController> controller,
      int line_number);

  // PopupInteractiveRowView:
  std::optional<CellType> GetSelectedCell() const override;
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

 private:
  // Marks the notice as acknowledged and removes it from the parent view.
  void OnGotItButtonClicked();

  // Opens personal context settings for autofill in Chrome settings.
  void OnSettingsLinkClicked();

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

  // The controller for the popup this notice is part of.
  const base::WeakPtr<AutofillPopupController> controller_;

  // The position of this notice in the vertical list of suggestions.
  const int line_number_;

  const raw_ref<PopupRowView::AccessibilitySelectionDelegate>
      a11y_selection_delegate_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_PERSONAL_CONTEXT_NOTICE_VIEW_H_
