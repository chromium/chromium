// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_ROW_LIST_VIEW_INFO_H_
#define CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_ROW_LIST_VIEW_INFO_H_

#include <list>
#include <map>
#include <optional>

#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/download/download_bubble_row_view_info.h"
#include "components/offline_items_collection/core/offline_item.h"

// Interface for observers of changes to the list of downloads as a whole.
class DownloadBubbleRowListViewInfoObserver : public base::CheckedObserver {
 public:
  DownloadBubbleRowListViewInfoObserver();
  ~DownloadBubbleRowListViewInfoObserver() override;

  // Called when a new row is added.
  virtual void OnRowAdded(const offline_items_collection::ContentId& id) {}

  // Called when a row is about to be removed. At this time, the row info for
  // `id` still exists.
  virtual void OnRowWillBeRemoved(
      const offline_items_collection::ContentId& id) {}

  // Called when a row has been removed.
  virtual void OnAnyRowRemoved() {}
};

// Info class for DownloadBubbleRowListView
class DownloadBubbleRowListViewInfo
    : public DownloadBubbleInfo<DownloadBubbleRowListViewInfoObserver>,
      public DownloadBubbleRowViewInfoObserver {
 public:
  using RowList = std::list<DownloadBubbleRowViewInfo>;

  explicit DownloadBubbleRowListViewInfo(
      std::vector<DownloadUIModel::DownloadUIModelPtr> models);
  ~DownloadBubbleRowListViewInfo() override;

  const RowList& rows() const { return rows_; }

  // Returns pointer to row info with `id`, or nullptr if no row is found.
  const DownloadBubbleRowViewInfo* GetRowInfo(
      const offline_items_collection::ContentId& id) const;

  std::optional<base::Time> last_completed_time() const {
    return last_completed_time_;
  }

  void AddRow(DownloadUIModel::DownloadUIModelPtr model);
  void RemoveRow(const offline_items_collection::ContentId& id);

 private:
  using RowListIterMap =
      std::map<offline_items_collection::ContentId, RowList::iterator>;

  // DownloadBubbleRowViewInfoObserver implementation:
  void OnDownloadDestroyed(const ContentId& id) override;

  RowList rows_;
  // Maps ContentId to iterator in `rows_`. We need this map because, during the
  // deletion of a row, the ContentId is no longer accessible from the model
  // (because the pointer to the download has been set to null), so we need a
  // way to locate the correct info to remove, given a ContentId.
  RowListIterMap row_list_iter_map_;
  std::optional<base::Time> last_completed_time_;
};

#endif  // CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_ROW_LIST_VIEW_INFO_H_
