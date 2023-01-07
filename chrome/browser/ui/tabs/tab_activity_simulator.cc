// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_activity_simulator.h"

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"

TabActivitySimulator::TabActivitySimulator() = default;
TabActivitySimulator::~TabActivitySimulator() = default;

void TabActivitySimulator::Navigate(content::WebContents* web_contents,
                                    const GURL& url,
                                    ui::PageTransition page_transition) {
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents);
  navigation->SetTransition(page_transition);
  navigation->SetKeepLoading(true);
  navigation->Commit();
}

std::unique_ptr<content::WebContents> TabActivitySimulator::CreateWebContents(
    content::BrowserContext* browser_context,
    bool initially_visible) {
  content::WebContents::CreateParams params(browser_context);
  params.initially_hidden = !initially_visible;
  std::unique_ptr<content::WebContents> test_contents(
      content::WebContentsTester::CreateTestWebContents(params));

  return test_contents;
}

content::WebContents* TabActivitySimulator::AddWebContentsAndNavigate(
    TabStripModel* tab_strip_model,
    const GURL& initial_url,
    ui::PageTransition page_transition) {
  // Create as a foreground tab if it's the only tab in the tab strip.
  bool initially_visible = tab_strip_model->empty();
  std::unique_ptr<content::WebContents> test_contents =
      CreateWebContents(tab_strip_model->profile(), initially_visible);
  content::WebContents* raw_test_contents = test_contents.get();
  tab_strip_model->AppendWebContents(std::move(test_contents),
                                     initially_visible /* foreground */);
  Navigate(raw_test_contents, initial_url, page_transition);
  return raw_test_contents;
}

void TabActivitySimulator::SwitchToTabAt(TabStripModel* tab_strip_model,
                                         int new_index) {
  int active_index = tab_strip_model->active_index();
  CHECK(new_index != active_index);

  content::WebContents* active_contents =
      tab_strip_model->GetWebContentsAt(active_index);
  CHECK(active_contents);
  content::WebContents* new_contents =
      tab_strip_model->GetWebContentsAt(new_index);
  CHECK(new_contents);

  // Activate the tab. Normally this would hide the active tab's aura::Window,
  // which is what actually triggers TabActivityWatcher to log the change. For
  // a TestWebContents, we must manually call WasHidden(), and do the reverse
  // for the newly activated tab.
  tab_strip_model->ActivateTabAt(
      new_index, TabStripUserGestureDetails(
                     TabStripUserGestureDetails::GestureType::kOther));
  active_contents->WasHidden();
  new_contents->WasShown();
}
