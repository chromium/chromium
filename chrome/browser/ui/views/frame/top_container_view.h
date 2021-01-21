// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTAINER_VIEW_H_

#include "base/compiler_specific.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class BrowserView;

// Container for the BrowserView's tab strip, toolbar, and sometimes bookmark
// bar. In Chrome OS immersive fullscreen it stacks on top of other views in
// order to slide in and out over the web contents.
class TopContainerView : public views::View {
 public:
  METADATA_HEADER(TopContainerView);
  explicit TopContainerView(BrowserView* browser_view);
  TopContainerView(const TopContainerView&) = delete;
  TopContainerView& operator=(const TopContainerView&) = delete;
  ~TopContainerView() override;

  // views::View overrides:
  void PaintChildren(const views::PaintInfo& paint_info) override;
  void ChildPreferredSizeChanged(views::View* child) override;

 private:
  // The parent of this view. Not owned.
  BrowserView* browser_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTAINER_VIEW_H_
