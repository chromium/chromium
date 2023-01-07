// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_VIEW_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_VIEW_OBSERVER_H_

class PageActionIconView;

class PageActionIconViewObserver {
 public:
  // Called after PageActionIconView::SetVisible() if going from false to true.
  virtual void OnPageActionIconViewShown(PageActionIconView* view) {}

  // Called at the beginning of PageActionIconView::NotifyClick().
  virtual void OnPageActionIconViewClicked(PageActionIconView* view) {}

 protected:
  virtual ~PageActionIconViewObserver() = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_VIEW_OBSERVER_H_
