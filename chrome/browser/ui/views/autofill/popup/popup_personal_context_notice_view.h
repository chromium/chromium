// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_PERSONAL_CONTEXT_NOTICE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_PERSONAL_CONTEXT_NOTICE_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class MdTextButton;
}

namespace autofill {

class AutofillPopupController;

// The view that displays the "Personal context" notice.
// This notice is shown at the bottom of the Autofill popup to inform the
// user that personal context is enabled.
class PopupPersonalContextNoticeView : public views::View {
  METADATA_HEADER(PopupPersonalContextNoticeView, views::View)

 public:
  explicit PopupPersonalContextNoticeView(
      base::WeakPtr<AutofillPopupController> controller,
      int line_number);
  PopupPersonalContextNoticeView(const PopupPersonalContextNoticeView&) =
      delete;
  PopupPersonalContextNoticeView& operator=(
      const PopupPersonalContextNoticeView&) = delete;
  ~PopupPersonalContextNoticeView() override;

  views::MdTextButton* got_it_button_for_testing() { return got_it_button_; }

 private:
  // Marks the notice as acknowledged and removes it from the parent view.
  void OnGotItButtonClicked();

  // The button users click to acknowledge the notice.
  raw_ptr<views::MdTextButton> got_it_button_ = nullptr;

  // The controller for the popup this notice is part of.
  base::WeakPtr<AutofillPopupController> controller_;

  // The position of this notice in the vertical list of suggestions.
  int line_number_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_PERSONAL_CONTEXT_NOTICE_VIEW_H_
