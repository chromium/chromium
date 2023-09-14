// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_PRIMARY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_PRIMARY_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "chrome/browser/download/download_ui_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

class Browser;
class DownloadBubbleNavigationHandler;
class DownloadBubbleRowListView;
class DownloadBubbleRowView;
class DownloadBubbleUIController;

namespace offline_items_collection {
struct ContentId;
}

namespace views {
class ScrollView;
}  // namespace views

// Base class for either type of primary view (partial or main). Consists of
// an optional header, a scrolling view for the download rows, and an optional
// footer.
class DownloadBubblePrimaryView : public views::FlexLayoutView {
 public:
  METADATA_HEADER(DownloadBubblePrimaryView);

  DownloadBubblePrimaryView();
  ~DownloadBubblePrimaryView() override;

  DownloadBubblePrimaryView(const DownloadBubblePrimaryView&) = delete;
  DownloadBubblePrimaryView& operator=(const DownloadBubblePrimaryView&) =
      delete;

  // Gets the row view with the given id. Returns nullptr if not found.
  DownloadBubbleRowView* GetRow(const offline_items_collection::ContentId& id);

  // Gets the row view at the given index.
  DownloadBubbleRowView* GetRowForTesting(size_t index);

  views::ScrollView* scroll_view_for_testing() { return scroll_view_; }

 protected:
  // TODO(crbug.com/1344515): Add support for refreshing the scroll view
  // contents.
  void BuildAndAddScrollView(
      base::WeakPtr<Browser> browser,
      base::WeakPtr<DownloadBubbleUIController> bubble_controller,
      base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
      std::vector<DownloadUIModel::DownloadUIModelPtr> models,
      int fixed_width);

  int DefaultPreferredWidth() const;

  // Maybe show the banner informing the user that any files downloaded
  // in OTR mode are visible to anyone on the device.
  void MaybeAddOtrInfoRow(Browser* browser);

  // Log the histogram for how long the bubble was visible.
  void LogVisibleTimeMetrics() const;
  virtual base::StringPiece GetVisibleTimeHistogramName() const = 0;

 private:
  // The ScrollView holding the DownloadBubbleRowListView with the download
  // rows.
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;
  // The contents contained in the above `scroll_view_`.
  raw_ptr<DownloadBubbleRowListView> row_list_view_ = nullptr;

  // Time when this view was created, for metrics.
  base::Time creation_time_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_PRIMARY_VIEW_H_
