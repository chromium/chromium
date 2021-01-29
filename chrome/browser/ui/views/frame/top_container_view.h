// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTAINER_VIEW_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/views/view.h"

class BrowserView;

// Container for the BrowserView's tab strip, toolbar, and sometimes bookmark
// bar. In Chrome OS immersive fullscreen it stacks on top of other views in
// order to slide in and out over the web contents.
class TopContainerView : public views::View {
 public:
  explicit TopContainerView(BrowserView* browser_view);
  ~TopContainerView() override;

  // views::View overrides:
  const char* GetClassName() const override;
  void PaintChildren(const views::PaintInfo& paint_info) override;
  void ChildPreferredSizeChanged(views::View* child) override;

 private:
  // The parent of this view. Not owned.
  BrowserView* browser_view_;

  DISALLOW_COPY_AND_ASSIGN(TopContainerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTAINER_VIEW_H_
