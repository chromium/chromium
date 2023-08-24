// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_TAB_MENU_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_BROWSER_TAB_MENU_MODEL_DELEGATE_H_

#include <vector>
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"

class Browser;

namespace chrome {

// Implementation of TabMenuModelDelegate which uses an instance of Browser to
// fulfill its duties.
class BrowserTabMenuModelDelegate : public TabMenuModelDelegate {
 public:
  explicit BrowserTabMenuModelDelegate(Browser* browser);
  ~BrowserTabMenuModelDelegate() override;

 private:
  // TabMenuModelDelegate:
  std::vector<Browser*> GetOtherBrowserWindows(bool is_app) override;

  const raw_ptr<Browser, DanglingUntriaged> browser_;
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_TAB_MENU_MODEL_DELEGATE_H_
