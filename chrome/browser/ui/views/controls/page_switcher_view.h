// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTROLS_PAGE_SWITCHER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CONTROLS_PAGE_SWITCHER_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

// A container view that contains one view at a time and can switch between
// views with animation.
// TODO(crbug.com/40754666): Implement animation when switching.
class PageSwitcherView : public views::View {
  METADATA_HEADER(PageSwitcherView, views::View)

 public:
  explicit PageSwitcherView(std::unique_ptr<views::View> initial_page);
  PageSwitcherView(const PageSwitcherView&) = delete;
  PageSwitcherView& operator=(const PageSwitcherView&) = delete;
  ~PageSwitcherView() override;

  // Removes all child views and adds `page` instead.
  void SwitchToPage(std::unique_ptr<views::View> page);

  // Returns the current page shown.
  views::View* GetCurrentPage();

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;

 private:
  raw_ptr<views::View, DanglingUntriaged> current_page_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */, PageSwitcherView, views::View)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, PageSwitcherView)

#endif  // CHROME_BROWSER_UI_VIEWS_CONTROLS_PAGE_SWITCHER_VIEW_H_
