// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"

#include "base/check_is_test.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/commerce/product_specifications_entry_point_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/lens/lens_features.h"

namespace {

// This is the generic entry point for test code to stub out browser window
// functionality. It is called by production code, but only used by tests.
BrowserWindowFeatures::BrowserWindowFeaturesFactory& GetFactory() {
  static base::NoDestructor<BrowserWindowFeatures::BrowserWindowFeaturesFactory>
      factory;
  return *factory;
}

}  // namespace

// static
std::unique_ptr<BrowserWindowFeatures>
BrowserWindowFeatures::CreateBrowserWindowFeatures() {
  if (GetFactory()) {
    CHECK_IS_TEST();
    return GetFactory().Run();
  }
  // Constructor is protected.
  return base::WrapUnique(new BrowserWindowFeatures());
}

BrowserWindowFeatures::~BrowserWindowFeatures() = default;

// static
void BrowserWindowFeatures::ReplaceBrowserWindowFeaturesForTesting(
    BrowserWindowFeaturesFactory factory) {
  BrowserWindowFeatures::BrowserWindowFeaturesFactory& f = GetFactory();
  f = std::move(factory);
}

void BrowserWindowFeatures::Init(Browser* browser) {
  // Avoid passing `browser` directly to features. Instead, pass the minimum
  // necessary state or controllers necessary.
  // Ping erikchen for assistance. This comment will be deleted after there are
  // 10+ features.
  //
  // Features that are only enabled for normal browser windows (e.g. a window
  // with an omnibox and a tab strip). By default most features should be
  // instantiated in this block.
  if (browser->is_type_normal()) {
    product_specifications_entry_point_controller_ =
        std::make_unique<commerce::ProductSpecificationsEntryPointController>(
            browser);
  }
}

void BrowserWindowFeatures::InitPostWindowConstruction(Browser* browser) {
  // Features that are only enabled for normal browser windows (e.g. a window
  // with an omnibox and a tab strip). By default most features should be
  // instantiated in this block.
  if (browser->is_type_normal()) {
    // TODO(b/350508658): Ideally, we don't pass in a reference to browser as
    // per the guidance in the comment above. However, currently, we need
    // browser to properly determine if the lens overlay is enabled.
    // Cannot be in Init since needs to listen to the fullscreen controller
    // which is initialized after Init.
    if (lens::features::IsLensOverlayEnabled()) {
      lens_overlay_entry_point_controller_ =
          std::make_unique<lens::LensOverlayEntryPointController>(browser);
    }
  }
}

void BrowserWindowFeatures::InitPostBrowserViewConstruction(
    BrowserView* browser_view) {
  // TODO(crbug.com/346148093): Move SidePanelCoordinator construction to Init.
  // TODO(crbug.com/346148554): Do not create a SidePanelCoordinator for most
  // browser.h types
  side_panel_coordinator_ =
      std::make_unique<SidePanelCoordinator>(browser_view);
}

void BrowserWindowFeatures::TearDownPreBrowserViewDestruction() {
  // TODO(crbug.com/346148093): This logic should not be gated behind a
  // conditional.
  if (side_panel_coordinator_) {
    side_panel_coordinator_->TearDownPreBrowserViewDestruction();
  }
}

SidePanelUI* BrowserWindowFeatures::side_panel_ui() {
  return side_panel_coordinator_.get();
}

BrowserWindowFeatures::BrowserWindowFeatures() = default;
