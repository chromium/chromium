// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/trigger_observer.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/organization/trigger.h"
#include "content/public/browser/browser_context.h"

TabOrganizationTriggerObserver::TabOrganizationTriggerObserver(
    base::RepeatingCallback<void(const Browser*)> on_trigger,
    content::BrowserContext* browser_context,
    std::unique_ptr<TabOrganizationTrigger> trigger_logic)
    : trigger_logic_(std::move(trigger_logic)),
      on_trigger_(on_trigger),
      browser_context_(browser_context) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    OnBrowserAdded(browser);
  }
  BrowserList::GetInstance()->AddObserver(this);
}

TabOrganizationTriggerObserver::~TabOrganizationTriggerObserver() {
  BrowserList::GetInstance()->RemoveObserver(this);
  // TabStripModelObserver destructor will stop observing the TabStripModels.
}

void TabOrganizationTriggerObserver::OnBrowserAdded(Browser* browser) {
  if (browser_context_ != browser->profile()) {
    return;
  }

  tab_strip_model_to_browser_map_.emplace(browser->tab_strip_model(), browser);
  browser->tab_strip_model()->AddObserver(this);
}

void TabOrganizationTriggerObserver::OnBrowserRemoved(Browser* browser) {
  if (browser_context_ != browser->profile()) {
    return;
  }

  tab_strip_model_to_browser_map_.erase(browser->tab_strip_model());
  browser->tab_strip_model()->RemoveObserver(this);
}

void TabOrganizationTriggerObserver::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (trigger_logic_->ShouldTrigger(tab_strip_model)) {
    on_trigger_.Run(BrowserForTabStripModel(tab_strip_model));
  }
}

Browser* TabOrganizationTriggerObserver::BrowserForTabStripModel(
    TabStripModel* tab_strip_model) const {
  return tab_strip_model_to_browser_map_.at(tab_strip_model).get();
}
