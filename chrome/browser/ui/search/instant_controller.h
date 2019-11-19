// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_INSTANT_CONTROLLER_H_
#define CHROME_BROWSER_UI_SEARCH_INSTANT_CONTROLLER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

#if defined(OS_ANDROID)
#error "Instant is only used on desktop";
#endif

class Profile;
class TabStripModel;

namespace content {
class WebContents;
}  // namespace content

// InstantController is responsible for updating the theme and most visited info
// of the current tab when
// * the user switches to an existing NTP tab, or
// * an open tab navigates to an NTP.
//
// InstantController is owned by Browser via BrowserInstantController.
class InstantController : public TabStripModelObserver {
 public:
  explicit InstantController(Profile* profile, TabStripModel* tab_strip_model);
  ~InstantController() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  class TabObserver;

  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest,
                           DispatchMVChangeEventWhileNavigatingBackToNTP);

  void StartWatchingTab(content::WebContents* web_contents);
  void StopWatchingTab(content::WebContents* web_contents);

  // Sends theme info and most visited items to the Instant renderer process.
  void UpdateInfoForInstantTab();

  Profile* const profile_;

  // Observes the currently active tab, and calls us back if it becomes an NTP.
  std::unique_ptr<TabObserver> tab_observer_;

  DISALLOW_COPY_AND_ASSIGN(InstantController);
};

#endif  // CHROME_BROWSER_UI_SEARCH_INSTANT_CONTROLLER_H_
