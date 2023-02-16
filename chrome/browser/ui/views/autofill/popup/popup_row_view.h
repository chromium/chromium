// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace autofill {

class PopupRowStrategy;
class PopupViewViews;

// `PopupRowView` represents a single selectable popup row. Different styles
// of the row can be achieved by injecting the respective `PopupRowStrategy`
// objects in the constructor.
// TODO(crbug.com/1411172): Add support for (selectable) control areas in the
// row.
class PopupRowView : public views::View {
 public:
  METADATA_HEADER(PopupRowView);
  PopupRowView(PopupViewViews& popup_view,
               std::unique_ptr<PopupRowStrategy> strategy);
  PopupRowView(const PopupRowView&) = delete;
  PopupRowView& operator=(const PopupRowView&) = delete;
  ~PopupRowView() override;

  // Acts as a factory method for creating a row view.
  static std::unique_ptr<PopupRowView> Create(PopupViewViews& popup_view,
                                              int line_number);

  // Gets and sets the selection status of the row.
  bool GetSelected() const { return selected_; }
  void SetSelected(bool selected);

  // Show the in-product-help promo anchored to this bubble if applicable. The
  // in-product-help promo is a bubble anchored to this item to show educational
  // messages. The promo bubble should only be shown once in one session and has
  // a limit for how many times it can be shown at most in a period of time.
  void MaybeShowIphPromo();

  // Returns the view representing the content area of the row.
  PopupCellView& GetContentView() { return *content_view_; }

 private:
  PopupViewViews& GetPopupView() { return popup_view_.get(); }

  // The parent view containing this row.
  const raw_ref<PopupViewViews> popup_view_;
  const std::unique_ptr<PopupRowStrategy> strategy_;

  // Whether this row is currently selected.
  bool selected_ = false;

  // The cell wrapping the content area of the row.
  raw_ptr<PopupCellView> content_view_ = nullptr;
  // TODO(crbug.com/1411172): Add a control view.
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_VIEW_H_
