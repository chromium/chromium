// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_REGION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_REGION_VIEW_H_

#include "ui/views/view.h"

// Container for the tabstrip, new tab button, and reserved grab handle space.
// TODO (https://crbug.com/949660) Under construction.
class TabStripRegionView final : public views::View {
 public:
  TabStripRegionView();
  ~TabStripRegionView() override;

  // views::View overrides:
  const char* GetClassName() const override;
  void ChildPreferredSizeChanged(views::View* child) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabStripRegionView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_REGION_VIEW_H_
