// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/custom_state_observers.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/base/interaction/state_observer.h"

namespace supervised_user {

TabTitleObserver::TabTitleObserver(Browser* browser, int observed_tab_index)
    : browser_(browser), observed_tab_index_(observed_tab_index) {
  browser_->tab_strip_model()->AddObserver(this);
}

TabTitleObserver::~TabTitleObserver() {
  if (browser_ && browser_->tab_strip_model()) {
    browser_->tab_strip_model()->RemoveObserver(this);
  }
}

void TabTitleObserver::TabChangedAt(content::WebContents* contents,
                                    int index,
                                    TabChangeType change_type) {
  if (index != observed_tab_index_) {
    return;
  }
  OnStateObserverStateChanged(base::UTF16ToWide(contents->GetTitle()));
}

void TabTitleObserver::OnTabStripModelDestroyed(
    TabStripModel* tab_strip_model) {
  tab_strip_model->RemoveObserver(this);
}

}  // namespace supervised_user
