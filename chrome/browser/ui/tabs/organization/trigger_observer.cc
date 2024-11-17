// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/trigger_observer.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/organization/trigger.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

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
  // Debounce TabStripModelObserver notifications - some batch tabstrip changes
  // can send out many separate notifications.
  if (!pending_eval_) {
    pending_eval_ = true;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&TabOrganizationTriggerObserver::EvaluateTrigger,
                       weak_ptr_factory_.GetWeakPtr(),
                       BrowserForTabStripModel(tab_strip_model)->AsWeakPtr()));
  }
}

Browser* TabOrganizationTriggerObserver::BrowserForTabStripModel(
    TabStripModel* tab_strip_model) const {
  return tab_strip_model_to_browser_map_.at(tab_strip_model).get();
}

void TabOrganizationTriggerObserver::EvaluateTrigger(
    base::WeakPtr<Browser> browser) {
  if (!browser) {
    return;
  }
  if (trigger_logic_->ShouldTrigger(browser->tab_strip_model())) {
    on_trigger_.Run(BrowserForTabStripModel(browser->tab_strip_model()));
  }

  pending_eval_ = false;
}
