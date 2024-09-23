// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BOCA_CHROME_TAB_STRIP_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_BOCA_CHROME_TAB_STRIP_DELEGATE_H_

#include <vector>

#include "ash/public/cpp/tab_strip_delegate.h"

class ChromeTabStripDelegate : public ash::TabStripDelegate {
 public:
  ChromeTabStripDelegate();
  ChromeTabStripDelegate(const ChromeTabStripDelegate&) = delete;
  ChromeTabStripDelegate& operator=(const ChromeTabStripDelegate&) = delete;
  ~ChromeTabStripDelegate() override;

  // ash::TabStripDelegate:
  std::vector<ash::TabInfo> GetTabsListForWindow(
      aura::Window* window) const override;
};

#endif  // CHROME_BROWSER_UI_ASH_BOCA_CHROME_TAB_STRIP_DELEGATE_H_
