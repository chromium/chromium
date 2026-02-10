// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_DIVIDER_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_DIVIDER_H_

#include "ui/views/view.h"

// Properly-styled separator for dividers in the toolbar.
class ToolbarDivider : public views::View {
  METADATA_HEADER(ToolbarDivider, views::View)
 public:
  ToolbarDivider();
  ~ToolbarDivider() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_DIVIDER_H_
