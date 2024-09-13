// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_FEATURES_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_FEATURES_H_

#include <memory>

#include "base/functional/callback.h"

class Browser;
class BrowserView;
class BrowserWindowInterface;
class ChromeLabsCoordinator;
class SidePanelCoordinator;
class SidePanelUI;
class TabStripModel;
class ToastController;
class ToastService;

namespace extensions {
class Mv2DisabledDialogController;
}  // namespace extensions

namespace tabs {
class TabDeclutterController;
}  // namespace tabs

namespace commerce {
class ProductSpecificationsEntryPointController;
}  // namespace commerce

namespace lens {
class LensOverlayEntryPointController;
}  // namespace lens

namespace tab_groups {
class SessionServiceTabGroupSyncObserver;
}  // namespace tab_groups

// This class owns the core controllers for features that are scoped to a given
// browser window on desktop. It can be subclassed by tests to perform
// dependency injection.
class BrowserWindowFeatures {
 public:
  static std::unique_ptr<BrowserWindowFeatures> CreateBrowserWindowFeatures();
  virtual ~BrowserWindowFeatures();

  BrowserWindowFeatures(const BrowserWindowFeatures&) = delete;
  BrowserWindowFeatures& operator=(const BrowserWindowFeatures&) = delete;

  // Call this method to stub out BrowserWindowFeatures for tests.
  using BrowserWindowFeaturesFactory =
      base::RepeatingCallback<std::unique_ptr<BrowserWindowFeatures>()>;
  static void ReplaceBrowserWindowFeaturesForTesting(
      BrowserWindowFeaturesFactory factory);

  // Called exactly once to initialize features. This is called prior to
  // instantiating BrowserView, to allow the view hierarchy to depend on state
  // in this class.
  void Init(BrowserWindowInterface* browser);

  // Called exactly once to initialize features that depend on the window object
  // being created.
  void InitPostWindowConstruction(Browser* browser);

  // Called exactly once to initialize features that depend on the view
  // hierarchy in BrowserView.
  void InitPostBrowserViewConstruction(BrowserView* browser_view);

  // Called exactly once to tear down state that depends on BrowserView.
  void TearDownPreBrowserViewDestruction();

  // Public accessors for features:
  commerce::ProductSpecificationsEntryPointController*
  product_specifications_entry_point_controller() {
    return product_specifications_entry_point_controller_.get();
  }
  extensions::Mv2DisabledDialogController*
  mv2_disabled_dialog_controller_for_testing() {
    return mv2_disabled_dialog_controller_.get();
  }

  ChromeLabsCoordinator* chrome_labs_coordinator() {
    return chrome_labs_coordinator_.get();
  }

  // TODO(crbug.com/346158959): For historical reasons, side_panel_ui is an
  // abstract base class that contains some, but not all of the public interface
  // of SidePanelCoordinator. One of the accessors side_panel_ui() or
  // side_panel_coordinator() should be removed. For consistency with the rest
  // of this class, we use lowercase_with_underscores even though the
  // implementation is not inlined.
  SidePanelUI* side_panel_ui();

  SidePanelCoordinator* side_panel_coordinator() {
    return side_panel_coordinator_.get();
  }

  lens::LensOverlayEntryPointController* lens_overlay_entry_point_controller() {
    return lens_overlay_entry_point_controller_.get();
  }

  tabs::TabDeclutterController* tab_declutter_controller() {
    return tab_declutter_controller_.get();
  }

  TabStripModel* tab_strip_model() { return tab_strip_model_; }

  // Returns a pointer to the ToastController for the browser window. This can
  // return nullptr for non-normal browser windows because toasts are not
  // supported for those cases.
  ToastController* toast_controller();

  // Returns a pointer to the ToastService for the browser window. This can
  // return nullptr for non-normal browser windows because toasts are not
  // supported for those cases.
  ToastService* toast_service() { return toast_service_.get(); }

 protected:
  BrowserWindowFeatures();

  // Override these methods to stub out individual feature controllers for
  // testing. e.g.
  // virtual std::unique_ptr<FooFeature> CreateFooFeature();

 private:
  // Features that are per-browser window will each have a controller. e.g.
  // std::unique_ptr<FooFeature> foo_feature_;

  std::unique_ptr<ChromeLabsCoordinator> chrome_labs_coordinator_;

  std::unique_ptr<commerce::ProductSpecificationsEntryPointController>
      product_specifications_entry_point_controller_;

  std::unique_ptr<lens::LensOverlayEntryPointController>
      lens_overlay_entry_point_controller_;

  std::unique_ptr<extensions::Mv2DisabledDialogController>
      mv2_disabled_dialog_controller_;

  std::unique_ptr<tabs::TabDeclutterController> tab_declutter_controller_;

  std::unique_ptr<SidePanelCoordinator> side_panel_coordinator_;

  std::unique_ptr<tab_groups::SessionServiceTabGroupSyncObserver>
      session_service_tab_group_sync_observer_;

  raw_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<ToastService> toast_service_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_FEATURES_H_
