// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_BNPL_FOOTNOTE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_BNPL_FOOTNOTE_VIEW_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Link;
}

namespace autofill {

class AutofillPopupController;

// A view that displays the privacy and legal footnote for Buy Now, Pay Later
// (BNPL) in the autofill popup. It handles dynamic bolding and embedded links.
class PopupBnplFootnoteView : public views::View {
  METADATA_HEADER(PopupBnplFootnoteView, views::View)

 public:
  PopupBnplFootnoteView(
      base::WeakPtr<AutofillPopupController> controller,
      PopupRowView::AccessibilitySelectionDelegate& a11y_selection_delegate,
      base::RepeatingCallback<void(const std::u16string&, bool)>
          announce_callback);
  PopupBnplFootnoteView(const PopupBnplFootnoteView&) = delete;
  PopupBnplFootnoteView& operator=(const PopupBnplFootnoteView&) = delete;
  ~PopupBnplFootnoteView() override;

  // Styles the settings link with a focused border to visually indicate
  // selection and announces the link text.
  void FocusSettingsLink();

  // Returns whether the settings link is currently visually focused/selected.
  bool IsSettingsLinkFocused() const;

  // Opens the Chrome payments settings subpage in a separate tab.
  void ActivateSettingsLink();

  // Resets the settings link border to an empty border to visually remove
  // focus.
  void UnfocusSettingsLink();

  const std::u16string& GetFullText() const { return full_text_; }

 private:
  views::Link* GetSettingsLink() const;

  std::u16string full_text_;
  bool is_settings_link_selected_ = false;
  base::WeakPtr<AutofillPopupController> controller_;
  base::RepeatingCallback<void(const std::u16string&, bool)> announce_callback_;
  const raw_ref<PopupRowView::AccessibilitySelectionDelegate>
      a11y_selection_delegate_;
  base::WeakPtrFactory<PopupBnplFootnoteView> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_BNPL_FOOTNOTE_VIEW_H_
