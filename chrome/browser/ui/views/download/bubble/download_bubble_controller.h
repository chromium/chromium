// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "content/public/browser/download_manager.h"

class DownloadBubbleUIController
    : public download::AllDownloadItemNotifier::Observer {
 public:
  explicit DownloadBubbleUIController(content::DownloadManager* manager);
  DownloadBubbleUIController(const DownloadBubbleUIController&) = delete;
  DownloadBubbleUIController& operator=(const DownloadBubbleUIController&) =
      delete;

  // AllDownloadItemNotifier::Observer
  void OnManagerGoingDown(content::DownloadManager* manager) override;

  // Get the main view of the Download Bubble. The main view contains all the
  // recent downloads (finished within the last 24 hours).
  std::unique_ptr<DownloadBubbleRowListView> GetMainView();

  // Get the partial view of the Download Bubble. The partial view contains
  // in-progress and uninteracted downloads, meant to capture the user's
  // recent tasks. This can only be opened by the browser in the event of new
  // downloads, and user action only creates a main view.
  std::unique_ptr<DownloadBubbleRowListView> GetPartialView();

 private:
  content::DownloadManager* download_manager_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_CONTROLLER_H_
