// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_NOTICE_VIEW_TEST_API_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_NOTICE_VIEW_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/autofill/popup/popup_notice_view.h"

namespace views {
class MdTextButton;
class StyledLabel;
}  // namespace views

namespace autofill {

class PopupNoticeViewTestApi {
 public:
  explicit PopupNoticeViewTestApi(PopupNoticeView* view) : view_(*view) {}

  views::StyledLabel* description() && { return view_->description_; }
  views::MdTextButton* accept_button() && { return view_->accept_button_; }
  bool is_link_focused() && { return view_->is_link_focused_; }
  bool is_accept_button_focused() && {
    return view_->is_accept_button_focused_;
  }

 private:
  const raw_ref<PopupNoticeView> view_;
};

inline PopupNoticeViewTestApi test_api(PopupNoticeView& view) {
  return PopupNoticeViewTestApi(&view);
}

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_NOTICE_VIEW_TEST_API_H_
