// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_OVERFLOW_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_OVERFLOW_BUTTON_H_

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

// A chevron button that indicates some toolbar elements have overflowed due to
// browser window being smaller than usual. Left press on it displays a drop
// down list of overflowed elements.
class OverflowButton : public ToolbarButton {
 public:
  METADATA_HEADER(OverflowButton);

  OverflowButton();
  OverflowButton(const OverflowButton&) = delete;
  OverflowButton& operator=(const OverflowButton&) = delete;
  ~OverflowButton() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_OVERFLOW_BUTTON_H_
