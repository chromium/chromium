// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_CONTAINER_H_

#include <memory>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

class PageActionIconController;
struct PageActionIconParams;

// Class implemented by a container view that holds page action icons.
class PageActionIconContainer {
 public:
  // Adds a page action icon to the container view. The container can
  // determine where to place and how to lay out the icons.
  virtual void AddPageActionIcon(std::unique_ptr<views::View> icon) = 0;
};

// Implements a default icon container for page action icons.
class PageActionIconContainerView : public views::BoxLayoutView,
                                    public PageActionIconContainer {
  METADATA_HEADER(PageActionIconContainerView, views::BoxLayoutView)

 public:
  explicit PageActionIconContainerView(const PageActionIconParams& params);
  PageActionIconContainerView(const PageActionIconContainerView&) = delete;
  PageActionIconContainerView& operator=(const PageActionIconContainerView&) =
      delete;
  ~PageActionIconContainerView() override;

  PageActionIconController* controller() { return controller_.get(); }

 private:
  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;

  // PageActionIconContainer:
  void AddPageActionIcon(std::unique_ptr<views::View> icon) override;

  std::unique_ptr<PageActionIconController> controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_CONTAINER_H_
