// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_NAVIGATION_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_NAVIGATION_HANDLER_H_

// An interface that provides methods to navigate between pages of the page
// info.
class PageInfoNavigationHandler {
 public:
  virtual void OpenMainPage() = 0;
  virtual void OpenSecurityPage() = 0;
  virtual void CloseBubble() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_NAVIGATION_HANDLER_H_
