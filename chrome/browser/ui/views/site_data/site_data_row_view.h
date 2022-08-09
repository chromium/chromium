// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SITE_DATA_SITE_DATA_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SITE_DATA_SITE_DATA_ROW_VIEW_H_

#include "ui/views/view.h"

class GURL;

// The view that represents a site that has acesss to the data or was blocked
// from accessing the data in the context of the currently visited website. The
// view is used as a row in the site data dialog. It contains a favicon (with a
// fallback icon), a hostname and a menu icon. The menu allows to change the
// cookies content setting for the site or delete the site data.
class SiteDataRowView : public views::View {
 public:
  explicit SiteDataRowView(const GURL& url);

 private:
  void OnMenuIconClicked();

  void OnDeleteMenuItemClicked(int event_flags);
  void OnBlockMenuItemClicked(int event_flags);
  void OnAllowMenuItemClicked(int event_flags);
  void OnClearOnExitMenuItemClicked(int event_flags);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SITE_DATA_SITE_DATA_ROW_VIEW_H_
