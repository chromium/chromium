// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WINDOW_SIZER_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_WINDOW_SIZER_LINUX_H_

#include "chrome/browser/ui/window_sizer/window_sizer.h"

class Browser;

class WindowSizerLinux : public WindowSizer {
 public:
  WindowSizerLinux(std::unique_ptr<StateProvider> state_provider,
                   const Browser* browser);
  WindowSizerLinux(const WindowSizerLinux&) = delete;
  WindowSizerLinux& operator=(const WindowSizerLinux&) = delete;
  ~WindowSizerLinux() override;

 protected:
  void AdjustWorkAreaForPlatform(gfx::Rect& work_area) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WINDOW_SIZER_LINUX_H_
