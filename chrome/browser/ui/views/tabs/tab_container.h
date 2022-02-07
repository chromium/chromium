// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

// A View that contains a sequence of Tabs for the TabStrip.
class TabContainer : public views::View {
 public:
  METADATA_HEADER(TabContainer);

  TabContainer() = default;
  ~TabContainer() override = default;

 private:
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_
