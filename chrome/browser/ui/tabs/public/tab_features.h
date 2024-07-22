// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_PUBLIC_TAB_FEATURES_H_
#define CHROME_BROWSER_UI_TABS_PUBLIC_TAB_FEATURES_H_

#include <memory>

#include "base/functional/callback.h"

class DipsNavigationFlowDetectorWrapper;
class LensOverlayController;
class Profile;

namespace customize_chrome {
class SidePanelController;
}

namespace permissions {
class PermissionIndicatorsTabData;
}

namespace tabs {

class TabInterface;

// This class owns the core controllers for features that are scoped to a given
// tab. It can be subclassed by tests to perform dependency injection.
class TabFeatures {
 public:
  static std::unique_ptr<TabFeatures> CreateTabFeatures();
  virtual ~TabFeatures();

  TabFeatures(const TabFeatures&) = delete;
  TabFeatures& operator=(const TabFeatures&) = delete;

  // Call this method to stub out TabFeatures for tests.
  using TabFeaturesFactory =
      base::RepeatingCallback<std::unique_ptr<TabFeatures>()>;
  static void ReplaceTabFeaturesForTesting(TabFeaturesFactory factory);

  LensOverlayController* lens_overlay_controller() {
    return lens_overlay_controller_.get();
  }

  permissions::PermissionIndicatorsTabData* permission_indicators_tab_data() {
    return permission_indicators_tab_data_.get();
  }

  customize_chrome::SidePanelController*
  customize_chrome_side_panel_controller() {
    return customize_chrome_side_panel_controller_.get();
  }

  DipsNavigationFlowDetectorWrapper* dips_navigation_flow_detector_wrapper() {
    return dips_navigation_flow_detector_wrapper_.get();
  }

  // Called exactly once to initialize features.
  void Init(TabInterface& tab, Profile* profile);

 protected:
  TabFeatures();

  // Override these methods to stub out individual feature controllers for
  // testing.
  virtual std::unique_ptr<LensOverlayController> CreateLensController(
      TabInterface* tab,
      Profile* profile);

 private:
  bool initialized_ = false;

  // Features that are per-tab will each have a controller.
  std::unique_ptr<LensOverlayController> lens_overlay_controller_;

  std::unique_ptr<permissions::PermissionIndicatorsTabData>
      permission_indicators_tab_data_;

  // Responsible for the customize chrome tab-scoped side panel.
  std::unique_ptr<customize_chrome::SidePanelController>
      customize_chrome_side_panel_controller_;

  std::unique_ptr<DipsNavigationFlowDetectorWrapper>
      dips_navigation_flow_detector_wrapper_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_PUBLIC_TAB_FEATURES_H_
