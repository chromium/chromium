// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BREADCRUMB_MANAGER_BROWSER_AGENT_H_
#define CHROME_BROWSER_UI_BREADCRUMB_MANAGER_BROWSER_AGENT_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/breadcrumbs/core/breadcrumb_manager_browser_agent.h"

class TabStripModel;
class TabStripModelChange;
struct TabStripSelectionChange;

namespace content {
class BrowserContext;
}  // namespace content

namespace breadcrumbs {
class BreadcrumbManagerKeyedService;
}  // namespace breadcrumbs

class BreadcrumbManagerBrowserAgent
    : public breadcrumbs::BreadcrumbManagerBrowserAgent,
      public TabStripModelObserver {
 public:
  BreadcrumbManagerBrowserAgent(TabStripModel* tab_strip_model,
                                content::BrowserContext* browser_context);
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

  const raw_ref<breadcrumbs::BreadcrumbManagerKeyedService> breadcrumb_manager_;
};

#endif  // CHROME_BROWSER_UI_BREADCRUMB_MANAGER_BROWSER_AGENT_H_
