// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TRIGGER_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TRIGGER_OBSERVER_H_

#include <memory>
#include <unordered_map>

#include "base/functional/callback.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/organization/trigger.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class TabStripModel;
class TabOrganizationTrigger;
namespace content {
class BrowserContext;
}

// Gets inputs for triggering, runs triggering logic when anything changes.
class TabOrganizationTriggerObserver : public BrowserListObserver,
                                       TabStripModelObserver {
 public:
  explicit TabOrganizationTriggerObserver(
      base::RepeatingCallback<void(const Browser*)> on_trigger,
      content::BrowserContext* browser_context,
      std::unique_ptr<TabOrganizationTrigger> trigger_logic);
  ~TabOrganizationTriggerObserver() override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  Browser* BrowserForTabStripModel(TabStripModel* tab_strip_model) const;

  std::unique_ptr<TabOrganizationTrigger> trigger_logic_;
  base::RepeatingCallback<void(const Browser*)> on_trigger_;
  raw_ptr<content::BrowserContext> browser_context_;
  std::unordered_map<TabStripModel*, raw_ptr<Browser>>
      tab_strip_model_to_browser_map_;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TRIGGER_OBSERVER_H_
