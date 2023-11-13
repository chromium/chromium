// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_CONTENT_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace autofill {

class PopupRowContentView : public views::View {
 public:
  METADATA_HEADER(PopupRowContentView);

  PopupRowContentView();
  PopupRowContentView(const PopupRowContentView&) = delete;
  PopupRowContentView& operator=(const PopupRowContentView&) = delete;
  ~PopupRowContentView() override;

  // Adds `label` to a list of labels whose style is refreshed whenever the
  // selection status of the cell changes. Assumes that `label` is a child of
  // `this` that will not be removed until `this` is destroyed.
  void TrackLabel(views::Label* label);

  // Updates the color of the view's background and adjusts the style of the
  // labels contained in it based on the `selected` value. When `selected` is
  // true the background color is set to `ui::kColorDropdownBackgroundSelected`,
  // otherwise it is transparent. The style of the text changes according to
  // the background color to keep it readable.
  void UpdateStyle(bool selected);

 private:
  // The labels whose style is updated when the cell's selection status changes.
  std::vector<raw_ptr<views::Label>> tracked_labels_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_CONTENT_VIEW_H_
