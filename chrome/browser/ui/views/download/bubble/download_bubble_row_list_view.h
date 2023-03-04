// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_LIST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_LIST_VIEW_H_

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

class Browser;
namespace views {
class ImageView;
}  // namespace views

class DownloadBubbleRowListView : public views::FlexLayoutView {
 public:
  METADATA_HEADER(DownloadBubbleRowListView);

  DownloadBubbleRowListView(
      bool is_partial_view,
      Browser* browser,
      base::OnceClosure on_mouse_entered_closure = base::DoNothing());
  ~DownloadBubbleRowListView() override;
  DownloadBubbleRowListView(const DownloadBubbleRowListView&) = delete;
  DownloadBubbleRowListView& operator=(const DownloadBubbleRowListView&) =
      delete;

  // views::FlexLayoutView
  void OnMouseEntered(const ui::MouseEvent& event) override;

 private:
  bool IsIncognitoInfoRowEnabled();

  bool is_partial_view_;
  base::Time creation_time_;
  raw_ptr<Browser> browser_ = nullptr;
  raw_ptr<views::ImageView> info_icon_ = nullptr;
  // Callback invoked when the user first hovers over the view.
  base::OnceClosure on_mouse_entered_closure_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_LIST_VIEW_H_
