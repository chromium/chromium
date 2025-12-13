// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_ENTRY_POINT_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_ENTRY_POINT_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_observer.h"
#include "chrome/browser/ui/lens/lens_url_matcher.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "ui/actions/actions.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view_observer.h"

class BrowserWindowInterface;
class CommandUpdater;

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace views {
class View;
}  // namespace views

namespace lens {

// Per-browser-window class responsible for keeping Lens Overlay entry points in
// their correct state. This functionality needs to be separate from
// LensOverlayController, since LensOverlayController exist per tab, while entry
// points are per browser window.
class LensOverlayEntryPointController : public FullscreenObserver,
                                        public TemplateURLServiceObserver,
                                        public views::FocusChangeListener,
                                        public views::ViewObserver {
 public:
  LensOverlayEntryPointController();
  ~LensOverlayEntryPointController() override;

  // This class does nothing if not initialized. IsEnabled returns false.
  void Initialize(BrowserWindowInterface* browser_window_interface,
                  CommandUpdater* command_updater,
                  views::View* location_bar);

  // Whether the entry points should be enabled. Enabled means the Lens Overlay
  // functionality is available.
  bool IsEnabled() const;

  // Returns true if the Lens Overlay entrypoints should be hidden. This is
  // different from IsEnabled() as IsEnabled() returns true if the Lens Overlay
  // functionality is available, while AreVisible() returns true if the Lens
  // Overlay functionality is available and the entrypoints should be visible at
  // this current moment in time. Sometimes, entrypoints are hidden ephermally,
  // such as when the Lens Overlay is currently active, so entrypoints do
  // nothing.
  bool AreVisible() const;

  // Updates the enable/disable and visibility state of entry points. If
  // hide_toolbar_entrypoint is true, instead of just disabling the toolbar
  // entrypoint, we will also hide the entrypoint from the user. All other
  // entrypoints will be updated to their correct state.
  void UpdateEntryPointsState(bool hide_toolbar_entrypoint);

  // Returns true if the given URL is eligible for EDU promos present on some
  // entrypoints.
  bool IsUrlEduEligible(const GURL& url) const;

  // Invokes the entrypoint action.
  static void InvokeAction(tabs::TabInterface* active_tab,
                           const actions::ActionInvocationContext& context);

 private:
  // FullscreenObserver:
  void OnFullscreenStateChanged() override;

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

  // views::FocusChangeListener
  void OnDidChangeFocus(views::View* before, views::View* now) override;

  // views::ViewObserver
  void OnViewAddedToWidget(views::View* view) override;
  void OnViewRemovedFromWidget(views::View* view) override;

  // Updates the Lens Overlay page action state.
  void UpdatePageActionState();
  bool ShouldShowPageAction(tabs::TabInterface* active_tab) const;

  // Returns the ActionItem corresponding to our pinnable toolbar entrypoint.
  actions::ActionItem* GetToolbarEntrypoint();

  // Return true if the Lens Overlay is active on the current tab.
  bool IsOverlayActive() const;

  // Observer to check for focus changes.
  base::ScopedObservation<views::FocusManager, views::FocusChangeListener>
      focus_manager_observation_{this};

  // Observer to check for browser window entering fullscreen.
  base::ScopedObservation<FullscreenController, FullscreenObserver>
      fullscreen_observation_{this};

  // Observer to check for changes to the users DSE.
  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      template_url_service_observation_{this};

  // Used to change whether the lens entrypoint is enabled in the 3 dot menu.
  // The CommandUpdater is owned by the BrowserWindowInterface, which also owns
  // this, and thus is guaranteed to outlive this.
  raw_ptr<CommandUpdater> command_updater_;

  // Owns this.
  raw_ptr<BrowserWindowInterface> browser_window_interface_;

  PrefChangeRegistrar pref_change_registrar_;

  raw_ptr<views::View> location_bar_;

  // URL matcher for entrypoints with EDU promos.
  std::unique_ptr<LensUrlMatcher> edu_url_matcher_;

  // Optimization guide decider used for determining EDU action chip
  // eligibility.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_{nullptr};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_ENTRY_POINT_CONTROLLER_H_
