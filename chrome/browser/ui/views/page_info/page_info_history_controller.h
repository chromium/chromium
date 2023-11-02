// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_HISTORY_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_HISTORY_CONTROLLER_H_

#include "ui/views/view_tracker.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace views {
class View;
}

namespace page_info {
class PageInfoHistoryDataSource;
}

// Controller for the history row located in `PageInfoMainView`.
// It fetches last visited information from `PageInfoHistoryDataSource` and
// creates a history button that opens a history webpage filtered to the site.
class PageInfoHistoryController {
 public:
  PageInfoHistoryController(content::WebContents* web_contents,
                            const GURL& site_url);
  ~PageInfoHistoryController();

  // Initializes the history row. A button will be added to `container` after
  // last visited information was fetched from `PageInfoHistoryDataSource`.
  void InitRow(views::View* container);

 private:
  // Creates a history button with `last_visit` information and adds to the
  // container accessed through `container_tracker_`. It clears the container
  // before adding a button to ensure that only one button exists at a time.
  void UpdateRow(base::Time last_visit);
  std::unique_ptr<views::View> CreateHistoryButton(std::u16string last_visit);
  void OpenHistoryPage();

  std::unique_ptr<page_info::PageInfoHistoryDataSource> history_data_source_;
  views::ViewTracker container_tracker_;
  raw_ptr<content::WebContents> web_contents_;
  GURL site_url_;

  base::WeakPtrFactory<PageInfoHistoryController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_HISTORY_CONTROLLER_H_
