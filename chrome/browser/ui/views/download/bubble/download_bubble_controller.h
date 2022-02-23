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

  std::unique_ptr<DownloadBubbleRowListView> GetMainView();

 private:
  content::DownloadManager* download_manager_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_CONTROLLER_H_
