// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SUPERVISED_USER_CUSTOM_STATE_OBSERVERS_H_
#define CHROME_TEST_SUPERVISED_USER_CUSTOM_STATE_OBSERVERS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/state_observer.h"

namespace supervised_user {

// StateObserver wrapper over TabStripModelObserver to fit into kombucha
// framework. Enables matching tabs by their title.
class TabTitleObserver : public ui::test::StateObserver<std::wstring>,
                         public TabStripModelObserver {
 public:
  explicit TabTitleObserver(Browser* browser, int observed_tab_index = 0);
  ~TabTitleObserver() override;

  TabTitleObserver(const TabTitleObserver& other) = delete;

  TabTitleObserver& operator=(const TabTitleObserver& other) = delete;

  // public TabStripModelObserver
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;
  void OnTabStripModelDestroyed(TabStripModel* tab_strip_model) override;

 private:
  raw_ptr<Browser> browser_;
  int observed_tab_index_;
};

}  // namespace supervised_user

#endif  // CHROME_TEST_SUPERVISED_USER_CUSTOM_STATE_OBSERVERS_H_
