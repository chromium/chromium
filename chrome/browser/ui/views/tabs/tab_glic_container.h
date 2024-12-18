// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GLIC_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GLIC_CONTAINER_H_

#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_observer.h"
#include "chrome/browser/ui/views/tabs/tab_search_container.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/common/buildflags.h"
#include "ui/views/view.h"

namespace glic {
class GlicButton;
}

class TabGlicContainer : public views::View, public TabDeclutterObserver {
  METADATA_HEADER(TabGlicContainer, views::View)
 public:
  explicit TabGlicContainer(
      TabStripController* tab_strip_controller,
      tabs::TabDeclutterController* tab_declutter_controller);
  TabGlicContainer(const TabGlicContainer&) = delete;
  TabGlicContainer& operator=(const TabGlicContainer&) = delete;
  ~TabGlicContainer() override;

  TabStripNudgeButton* tab_declutter_button() { return tab_declutter_button_; }

  // TabDeclutterObserver
  void OnTriggerDeclutterUIVisibility() override;
#if BUILDFLAG(ENABLE_GLIC)
  glic::GlicButton* GetGlicButton() { return glic_button_; }
#endif  // BUILDFLAG(ENABLE_GLIC)

 private:
  FRIEND_TEST_ALL_PREFIXES(TabGlicContainerBrowserTest,
                           LogsWhenDeclutterButtonDismissed);

  void ShowTabStripNudge(TabStripNudgeButton* button);
  void HideTabStripNudge(TabStripNudgeButton* button);

  void OnTabDeclutterButtonClicked();
  void OnTabDeclutterButtonDismissed();

  DeclutterTriggerCTRBucket GetDeclutterTriggerBucket(bool clicked);
  void LogDeclutterTriggerBucket(bool clicked);

  void ExecuteShowTabStripNudge(TabStripNudgeButton* button);
  void ExecuteHideTabStripNudge(TabStripNudgeButton* button);

  std::unique_ptr<TabStripNudgeButton> CreateTabDeclutterButton(
      TabStripController* tab_strip_controller);
  void SetupButtonProperties(TabStripNudgeButton* button);

  raw_ptr<TabStripNudgeButton> tab_declutter_button_ = nullptr;
  raw_ptr<tabs::TabDeclutterController> tab_declutter_controller_;
#if BUILDFLAG(ENABLE_GLIC)
  raw_ptr<glic::GlicButton, DanglingUntriaged> glic_button_ = nullptr;
#endif  // BUILDFLAG(ENABLE_GLIC)

  raw_ptr<const Browser> browser_;

  base::ScopedObservation<tabs::TabDeclutterController, TabDeclutterObserver>
      tab_declutter_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GLIC_CONTAINER_H_
