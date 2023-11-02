// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_LACROS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_LACROS_H_

#include "chrome/browser/ui/views/frame/browser_frame.h"

class BrowserView;

// BrowserFrameLacros provides the frame for Lacros browser windows.
class BrowserFrameLacros : public BrowserFrame {
 public:
  explicit BrowserFrameLacros(BrowserView* browser_view);

  // BrowserFrame
  bool ShouldDrawFrameHeader() const override;

 protected:
  ~BrowserFrameLacros() override = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_LACROS_H_
