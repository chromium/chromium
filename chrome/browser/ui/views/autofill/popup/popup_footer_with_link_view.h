// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_FOOTER_WITH_LINK_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_FOOTER_WITH_LINK_VIEW_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/autofill/popup/popup_interactive_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class Link;
class StyledLabel;
}  // namespace views

namespace autofill {

class AutofillPopupController;

// A footer view that displays text with an embedded link to a Chrome settings
// subpage and an optional leading icon.
class PopupFooterWithLinkView : public PopupInteractiveRowView {
  METADATA_HEADER(PopupFooterWithLinkView, PopupInteractiveRowView)

 public:
  PopupFooterWithLinkView(
      base::WeakPtr<AutofillPopupController> controller,
      PopupRowView::AccessibilitySelectionDelegate& a11y_selection_delegate,
      int text_id,
      int link_text_id,
      std::string_view settings_subpage,
      const gfx::VectorIcon* icon);
  ~PopupFooterWithLinkView() override;

  PopupFooterWithLinkView(const PopupFooterWithLinkView&) = delete;
  PopupFooterWithLinkView& operator=(const PopupFooterWithLinkView&) = delete;

  // PopupInteractiveRowView:
  std::optional<CellType> GetSelectedCell() const override;
  void SetSelectedCell(std::optional<CellType> cell) override;
  bool HandleKeyPressEvent(const input::NativeWebKeyboardEvent& event) override;
  bool IsSelectable() const override;

  // views::View:
  void Layout(views::View::PassKey pass_key) override;

 private:
  void OnLinkClicked();
  // Returns a vector since link text wrapped across lines is split
  // into multiple link views.
  std::vector<views::Link*> GetSettingsLinks() const;

  raw_ptr<views::StyledLabel> styled_label_ = nullptr;
  std::optional<CellType> selected_cell_;
  base::WeakPtr<AutofillPopupController> controller_;
  const raw_ref<PopupRowView::AccessibilitySelectionDelegate>
      a11y_selection_delegate_;
  const std::string settings_subpage_;

  base::WeakPtrFactory<PopupFooterWithLinkView> weak_ptr_factory_{this};
};

// Creates a `PopupFooterWithLinkView` for the AtMemory AI disclosure.
std::unique_ptr<PopupFooterWithLinkView> CreateAtMemoryAiDisclosureView(
    base::WeakPtr<AutofillPopupController> controller,
    PopupRowView::AccessibilitySelectionDelegate& a11y_selection_delegate);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_FOOTER_WITH_LINK_VIEW_H_
