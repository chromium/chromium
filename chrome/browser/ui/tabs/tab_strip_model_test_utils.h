// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_TEST_UTILS_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_TEST_UTILS_H_

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

const char kTabStripModelTestIDUserDataKey[] = "TabStripModelTestIDUserData";

class TabStripModelTestIDUserData : public base::SupportsUserData::Data {
 public:
  explicit TabStripModelTestIDUserData(int id) : id_(id) {}
  ~TabStripModelTestIDUserData() override = default;
  int id() { return id_; }

 private:
  int id_;
};

// Sets the id of the specified contents.
void SetID(WebContents* contents, int id);

// Returns the id of the specified contents.
int GetID(WebContents* contents);

void PrepareTabstripForSelectionTest(
    base::OnceCallback<void(int)> add_tabs_callback,
    TabStripModel* model,
    int tab_count,
    int pinned_count,
    const std::vector<int> selected_tabs);

// Returns the state of the given tab strip as a string. The state consists
// of the ID of each web contents followed by a 'p' if pinned, or an 's' if
// split. For example, if the model consists of four tabs with ids 0, 1, 2,
// and 3, with the first tab pinned and the last two split, this returns
// "0p 1 2s 3s".
std::string GetTabStripStateString(const TabStripModel* model,
                                   bool annotate_groups = false);

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_TEST_UTILS_H_
