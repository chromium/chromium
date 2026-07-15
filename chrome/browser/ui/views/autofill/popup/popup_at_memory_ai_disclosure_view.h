// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_AT_MEMORY_AI_DISCLOSURE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_AT_MEMORY_AI_DISCLOSURE_VIEW_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/autofill/popup/popup_interactive_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class Link;
class StyledLabel;
}  // namespace views

namespace autofill {

class AutofillPopupController;

// A footer view that displays an AI disclosure with a link.
class PopupAtMemoryAiDisclosureView : public PopupInteractiveRowView {
  METADATA_HEADER(PopupAtMemoryAiDisclosureView, PopupInteractiveRowView)

 public:
  PopupAtMemoryAiDisclosureView(
      base::WeakPtr<AutofillPopupController> controller,
      PopupRowView::AccessibilitySelectionDelegate& a11y_selection_delegate);
  ~PopupAtMemoryAiDisclosureView() override;

  PopupAtMemoryAiDisclosureView(const PopupAtMemoryAiDisclosureView&) = delete;
  PopupAtMemoryAiDisclosureView& operator=(
      const PopupAtMemoryAiDisclosureView&) = delete;

  // PopupInteractiveRowView:
  std::optional<CellType> GetSelectedCell() const override;
  void SetSelectedCell(std::optional<CellType> cell) override;
  bool HandleKeyPressEvent(const input::NativeWebKeyboardEvent& event) override;
  bool IsSelectable() const override;

  // views::View:
  void Layout(views::View::PassKey pass_key) override;

 private:
  void OnLearnMoreLinkClicked();
  // Returns a vector since link text wrapped across lines is split
  // into multiple link views.
  std::vector<views::Link*> GetSettingsLinks() const;

  raw_ptr<views::StyledLabel> styled_label_ = nullptr;
  std::optional<CellType> selected_cell_;
  base::WeakPtr<AutofillPopupController> controller_;
  const raw_ref<PopupRowView::AccessibilitySelectionDelegate>
      a11y_selection_delegate_;

  base::WeakPtrFactory<PopupAtMemoryAiDisclosureView> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_AT_MEMORY_AI_DISCLOSURE_VIEW_H_
