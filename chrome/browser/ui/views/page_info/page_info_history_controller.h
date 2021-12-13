// Copyright 2021 The Chromium Authors. All rights reserved.
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

class PageInfoHistoryController {
 public:
  PageInfoHistoryController(content::WebContents* web_contents,
                            const GURL& site_url);
  ~PageInfoHistoryController();

  void InitRow(views::View* container);

 private:
  void UpdateRow(base::Time last_visit);
  std::unique_ptr<views::View> CreateHistoryButton(std::u16string last_visit);
  void OpenHistoryPage();

  std::unique_ptr<page_info::PageInfoHistoryDataSource> history_data_source_;
  views::ViewTracker container_tracker_;
  raw_ptr<content::WebContents> web_contents_;

  base::WeakPtrFactory<PageInfoHistoryController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_HISTORY_CONTROLLER_H_
