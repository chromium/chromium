// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_VIEW_VIEWS_TEST_API_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_VIEW_VIEWS_TEST_API_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"

namespace content {
struct NativeWebKeyboardEvent;
}  // namespace content

namespace gfx {
class Rect;
}  // namespace gfx

namespace autofill {

class PopupViewViewsTestApi {
 public:
  explicit PopupViewViewsTestApi(PopupViewViews* view) : view_(*view) {}

  bool CanShowDropdownInBounds(const gfx::Rect& bounds) const&& {
    return view_->CanShowDropdownInBounds(bounds);
  }

  bool HandleKeyPressEvent(const content::NativeWebKeyboardEvent& event) && {
    return view_->HandleKeyPressEvent(event);
  }

  absl::optional<PopupViewViews::CellIndex> GetOpenSubPopupCell() const&& {
    return view_->open_sub_popup_cell_;
  }

  const std::vector<PopupViewViews::RowPointer>& rows() const&& {
    return view_->rows_;
  }

 private:
  raw_ref<PopupViewViews> view_;
};

inline PopupViewViewsTestApi test_api(PopupViewViews& view) {
  return PopupViewViewsTestApi(&view);
}

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_VIEW_VIEWS_TEST_API_H_
