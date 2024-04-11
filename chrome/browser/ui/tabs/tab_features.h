// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_FEATURES_H_
#define CHROME_BROWSER_UI_TABS_TAB_FEATURES_H_

#include <memory>

#include "base/functional/callback.h"

class LensOverlayController;

namespace tabs {

class TabModel;

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

  // Called exactly once to initialize features.
  void Init(TabModel* tab);

 protected:
  TabFeatures();

  // Override these methods to stub out individual feature controllers for
  // testing.
  virtual std::unique_ptr<LensOverlayController> CreateLensController(
      TabModel* tab);

 private:
  bool initialized_ = false;

  // Features that are per-tab will each have a controller.
  std::unique_ptr<LensOverlayController> lens_overlay_controller_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_FEATURES_H_
