// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/views/interaction/interaction_test_util_views.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kCustomizeChromeElementId);
}  // namespace

class WallpaperSearchInteractiveTest : public InteractiveBrowserTest {
 public:
  WallpaperSearchInteractiveTest() = default;
  ~WallpaperSearchInteractiveTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {ntp_features::kCustomizeChromeWallpaperSearch,
         optimization_guide::features::kOptimizationGuideModelExecution},
        {});
    InteractiveBrowserTest::SetUp();
  }

  StateChange Visible(const DeepQuery& where) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementVisibleEvent);
    StateChange state_change;
    state_change.type = StateChange::Type::kExistsAndConditionTrue;
    state_change.where = where;
    state_change.event = kElementVisibleEvent;
    state_change.test_function = "(el) => el.offsetParent !== null";
    return state_change;
  }

  InteractiveTestApi::MultiStep ClickElement(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element) {
    return Steps(WaitForStateChange(contents_id, Visible(element)),
                 MoveMouseTo(contents_id, element), ClickMouse());
  }

  InteractiveTestApi::MultiStep OpenCustomizeChrome() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabPageElementId);
    const DeepQuery kCustomizeChromeButton = {"ntp-app", "#customizeButton"};
    return Steps(
        // 1. Load the NTP.
        InstrumentTab(kNewTabPageElementId, 0),
        NavigateWebContents(kNewTabPageElementId,
                            GURL(chrome::kChromeUINewTabPageURL)),
        WaitForWebContentsReady(kNewTabPageElementId,
                                GURL(chrome::kChromeUINewTabPageURL)),
        // 2. Open Customize Chrome.
        ClickElement(kNewTabPageElementId, kCustomizeChromeButton),
        WaitForShow(kCustomizeChromeSidePanelWebViewElementId),
        // 3. Instrument Customize Chrome's WebUI.
        InstrumentNonTabWebView(kCustomizeChromeElementId,
                                kCustomizeChromeSidePanelWebViewElementId));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WallpaperSearchInteractiveTest,
                       OpenWallpaperSearchPage) {
  const DeepQuery kEditThemeButton = {"customize-chrome-app",
                                      "#appearanceElement", "#editThemeButton"};
  const DeepQuery kWallpaperSearchTile = {
      "customize-chrome-app", "#categoriesPage", "#wallpaperSearchTile"};
  const DeepQuery kWallpaperSearchPage = {"customize-chrome-app",
                                          "#wallpaperSearchPage"};
  RunTestSequence(
      // 1. Open Customize Chrome.
      OpenCustomizeChrome(),
      // 2. Open the theme categories page.
      ClickElement(kCustomizeChromeElementId, kEditThemeButton),
      // 3. Open wallpaper search.
      ClickElement(kCustomizeChromeElementId, kWallpaperSearchTile),
      // 4. Wallpaper search page should show.
      WaitForStateChange(kCustomizeChromeElementId,
                         Visible(kWallpaperSearchPage)));
}
