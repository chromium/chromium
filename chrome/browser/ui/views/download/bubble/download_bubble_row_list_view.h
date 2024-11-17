// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_LIST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_LIST_VIEW_H_

#include <map>
#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/download/download_bubble_row_list_view_info.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

class DownloadBubbleRowView;
class DownloadBubbleUIController;
class DownloadBubbleNavigationHandler;

class DownloadBubbleRowListView : public views::FlexLayoutView,
                                  public DownloadBubbleRowListViewInfoObserver {
  METADATA_HEADER(DownloadBubbleRowListView, views::FlexLayoutView)

 public:
  DownloadBubbleRowListView(
      base::WeakPtr<Browser> browser,
      base::WeakPtr<DownloadBubbleUIController> bubble_controller,
      base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
      int fixed_width,
      const DownloadBubbleRowListViewInfo& info,
      bool is_in_partial_view = false);
  ~DownloadBubbleRowListView() override;
  DownloadBubbleRowListView(const DownloadBubbleRowListView&) = delete;
  DownloadBubbleRowListView& operator=(const DownloadBubbleRowListView&) =
      delete;

  // TODO(crbug.com/40853007): Add functionality for adding a new download while
  // this is already open.

  // Removes a row and updates the `rows_by_id_` map. Returns ownership of the
  // row to the caller.
  std::unique_ptr<DownloadBubbleRowView> RemoveRow(DownloadBubbleRowView* row);

  // Gets the row for the download with the given id. Returns nullptr if not
  // found.
  DownloadBubbleRowView* GetRow(
      const offline_items_collection::ContentId& id) const;

  // Returns the number of rows.
  size_t NumRows() const;

  const DownloadBubbleRowListViewInfo& info() const { return *info_; }

 private:
  // DownloadBubbleRowListViewInfoObserver implementation:
  void OnRowAdded(const offline_items_collection::ContentId& id) override;
  void OnRowWillBeRemoved(
      const offline_items_collection::ContentId& id) override;

  // Adds a row to the bottom of the list.
  void AddRow(const DownloadBubbleRowViewInfo& row_info);

  // Map of download item's ID to child view in the row list.
  std::map<offline_items_collection::ContentId,
           raw_ptr<DownloadBubbleRowView, CtnExperimental>>
      rows_by_id_;

  base::WeakPtr<Browser> browser_;
  base::WeakPtr<DownloadBubbleUIController> bubble_controller_;
  base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler_;
  int fixed_width_ = 0;

  // Used for metrics to study clickjacking potential. False in tests.
  const bool is_in_partial_view_ = false;

  // This is owned by the DownloadBubbleContentsView owning `this`.
  raw_ref<const DownloadBubbleRowListViewInfo> info_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_LIST_VIEW_H_
