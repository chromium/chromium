// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BREADCRUMB_MANAGER_BROWSER_AGENT_H_
#define CHROME_BROWSER_UI_BREADCRUMB_MANAGER_BROWSER_AGENT_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/breadcrumbs/core/breadcrumb_manager_browser_agent.h"

class Browser;
class TabStripModel;
class TabStripModelChange;
struct TabStripSelectionChange;

class BreadcrumbManagerBrowserAgent
    : public breadcrumbs::BreadcrumbManagerBrowserAgent,
      public TabStripModelObserver {
 public:
  explicit BreadcrumbManagerBrowserAgent(Browser* browser);
  BreadcrumbManagerBrowserAgent(const BreadcrumbManagerBrowserAgent&) = delete;
  BreadcrumbManagerBrowserAgent& operator=(
      const BreadcrumbManagerBrowserAgent&) = delete;
  ~BreadcrumbManagerBrowserAgent() override;

 private:
  // breadcrumbs::BreadcrumbManagerBrowserAgent:
  void PlatformLogEvent(const std::string& event) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // The browser whose tab strip this agent observes. Can't be nullptr because
  // |browser_| owns this object.
  raw_ptr<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_BREADCRUMB_MANAGER_BROWSER_AGENT_H_
