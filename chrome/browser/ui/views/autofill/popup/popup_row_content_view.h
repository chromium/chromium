// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_CONTENT_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/layout/box_layout_view.h"

namespace autofill {

class PopupRowContentView : public views::BoxLayoutView {
  METADATA_HEADER(PopupRowContentView, views::BoxLayoutView)

 public:
  PopupRowContentView();
  PopupRowContentView(const PopupRowContentView&) = delete;
  PopupRowContentView& operator=(const PopupRowContentView&) = delete;
  ~PopupRowContentView() override;

  // Updates the color of the view's background and adjusts the style of the
  // labels contained in it based on the `selected` value. When `selected` is
  // true the background color is set to `ui::kColorDropdownBackgroundSelected`,
  // otherwise it is transparent. The style of the text changes according to
  // the background color to keep it readable.
  void UpdateStyle(bool selected);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_CONTENT_VIEW_H_
