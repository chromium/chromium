// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_immersive_activation_observer.h"
#include "chrome/browser/ui/read_anything/read_anything_lifecycle_observer.h"
#include "chrome/browser/ui/read_anything/read_anything_omnibox_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/common/read_anything/read_anything.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class ReadAnythingController;
class ReadAnythingService;

// A helper class to observe a specific WebContents, so the ReadAnything
// Controller can observe multiple WebContents. Event callbacks are configured
// by the instantiator.
class WebContentsObserverInstance : public content::WebContentsObserver {
 public:
  WebContentsObserverInstance(
      content::WebContents* web_contents,
      base::RepeatingClosure primary_page_changed_callback,
      base::RepeatingClosure renderer_crashed_callback,
      base::RepeatingCallback<void(content::Visibility)>
          visibility_changed_callback);

  WebContentsObserverInstance(const WebContentsObserverInstance&) = delete;
  WebContentsObserverInstance& operator=(const WebContentsObserverInstance&) =
      delete;
  ~WebContentsObserverInstance() override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void OnRendererUnresponsive(
      content::RenderProcessHost* render_process_host) override;

 private:
  base::RepeatingClosure primary_page_changed_callback_;
  base::RepeatingClosure renderer_crashed_callback_;
  base::RepeatingCallback<void(content::Visibility)>
      visibility_changed_callback_;
};

// Allows the Reading Mode WebUI (namely, the ReadAnythingUntrustedPageHandler)
// to lookup the ReadAnythingController from the WebUI's WebContents.
class ReadAnythingControllerGlue
    : public content::WebContentsUserData<ReadAnythingControllerGlue> {
 public:
  ~ReadAnythingControllerGlue() override = default;

  ReadAnythingController* controller() { return controller_; }

 private:
  friend class content::WebContentsUserData<ReadAnythingControllerGlue>;

  ReadAnythingControllerGlue(content::WebContents* contents,
                             ReadAnythingController* controller);

  const raw_ptr<ReadAnythingController> controller_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

// Manages the core logic for the Reading Mode feature.
//
// This controller is owned by TabFeatures and is instantiated once per tab.
// Its lifetime is tied to the TabInterface.
//
// It acts as the primary entry point for all Reading Mode commands and is
// responsible for orchestrating the display of the Reading Mode UI.
class ReadAnythingController : public tabs::ContentsObservingTabFeature {
 public:
  using Observer = ReadAnythingLifecycleObserver;

  ReadAnythingController(const ReadAnythingController&) = delete;
  ReadAnythingController& operator=(const ReadAnythingController&) = delete;
  ~ReadAnythingController() override;

  using PresentationState = read_anything::mojom::ReadAnythingPresentationState;

  using DistillationState = read_anything::mojom::ReadAnythingDistillationState;

  ReadAnythingController(tabs::TabInterface* tab,
                         SidePanelRegistry* side_panel_registry);

  DECLARE_USER_DATA(ReadAnythingController);
  static ReadAnythingController* From(tabs::TabInterface* tab);

  tabs::TabInterface* tab() { return tab_.get(); }

  // Adds/removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Add/removes observer responsible for handling immersive mode showing and
  // hiding.
  void AddImmersiveActivationObserver(
      ReadAnythingImmersiveActivationObserver* observer);
  void RemoveImmersiveActivationObserver(
      ReadAnythingImmersiveActivationObserver* observer);

  // Called when the WebUI is shown/hidden.
  void OnEntryShown(std::optional<ReadAnythingOpenTrigger> trigger);
  void OnEntryHidden();

  // Displays the Reading Mode UI in the Side Panel.
  void ShowSidePanelUI(SidePanelOpenTrigger trigger);

  // Displays the Immersive Reading Mode UI in a full screen overlay.
  void ShowImmersiveUI(ReadAnythingOpenTrigger trigger);

  // Closes the Immersive Reading Mode UI.
  void CloseImmersiveUI(bool closed_by_tab_switch = false);

  // Toggles the Immersive Reading Mode UI.
  void ToggleUI(ReadAnythingOpenTrigger trigger);

  // Toggles between the Immersive Reading Mode UI and the Side Panel UI.
  void TogglePresentation();

  // Toggles the Reading Mode Side Panel UI.
  void ToggleReadAnythingSidePanel(SidePanelOpenTrigger trigger);

  // Returns the current presentation_state_ of the Reading Mode feature. This
  // refers to the current host of the WebUI, but does not guarantee that the
  // feature is necessarily showing by the host.
  PresentationState GetPresentationState() const;

  void SetPresentationState(PresentationState new_state);

  void OnDistillationStateChanged(DistillationState new_state);

  // For testing only. Allows the distillation-related reactions to occur.
  void UnlockDistillationStateForTesting();

  // For testing only. Pauses distillation-related reactions from occurring.
  // Only affects new ReadAnythingController instances created after this flag
  // is set.
  static void SetFreezeDistillationOnCreationForTesting(bool locked);

  // Lazily creates and returns the WebUIContentsWrapper for the
  // Reading Mode WebUI. Transfers ownership of the WebUIContentsWrapper to the
  // caller, and the caller passes in the presentation that the webui will be
  // presented in (e.g. kInSidePanel, kInImmersiveOverlay)
  std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
  GetOrCreateWebUIWrapper(PresentationState web_ui_new_presentation_state);

  // Getter for has_shown_ui_. This is used by RM host views to
  // determine if the Reading Mode is ready to be shown, or if it should wait
  // for a notification that it is ready.
  bool has_shown_ui() const { return has_shown_ui_; }

  void SetWebUIWrapperForTest(
      std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
          web_ui_wrapper);

  // Called by other host views of the Reading Mode WebUI to return ownership of
  // the WebUIContentsWrapper to this controller.
  void TransferWebUiOwnership(
      std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
          web_ui_wrapper);

  // Recreates the WebUI on the next GetOrCreateWebUIWrapper() call. This should
  // be called if Reading mode crashes so that we don't get stuck in a crashed
  // state.
  void RecreateWebUIWrapper();

  // Artitficially sets the time when the user entered a page for testing the
  // omnibox entry point.
  void SetDwellTimeForTesting(base::TimeTicks test_time);

  ReadAnythingSidePanelController* GetSidePanelControllerForTesting() {
    return read_anything_side_panel_controller_.get();
  }

 private:
  // tabs::ContentsObservingTabFeature:
  void OnDiscardContents(tabs::TabInterface* tab,
                         content::WebContents* old_contents,
                         content::WebContents* new_contents) override;

  // Called when the tab will detach.
  void TabWillDetach(tabs::TabInterface* tab,
                     tabs::TabInterface::DetachReason reason);

  std::unique_ptr<WebContentsObserverInstance> ra_web_ui_observer_;
  std::unique_ptr<ReadAnythingOmniboxController> omnibox_controller_;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  // Callback for when ra_web_ui_observer_ receives a OnVisibilityChanged
  // event.
  void OnReadAnythingVisibilityChanged(content::Visibility visibility);

  // Callback for when ra_web_ui_observer_ determines the renderer has crashed
  // (e.g. due to being unresponsive).
  void OnRendererCrashed();

  // Returns the SidePanelUI for the active tab if it can be shown.
  // Otherwise, returns nullptr.
  SidePanelUI* GetSidePanelUI();

  raw_ptr<tabs::TabInterface> tab_ = nullptr;
  raw_ptr<SidePanelRegistry> side_panel_registry_ = nullptr;
  ui::ScopedUnownedUserData<ReadAnythingController> scoped_unowned_user_data_;

  std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
      web_ui_wrapper_;

  std::unique_ptr<ReadAnythingSidePanelController>
      read_anything_side_panel_controller_;

  bool has_shown_ui_ = false;
  bool should_recreate_web_ui_ = false;

  base::ObserverList<Observer> observers_;
  base::ObserverList<ReadAnythingImmersiveActivationObserver>
      immersive_activation_observers_;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  PresentationState presentation_state_ = PresentationState::kUndefined;
  // When a tab becomes inactive and hides IRM, this is set to true to let us
  // know we should show IRM again when the tab becomes active again.
  bool should_show_immersive_on_tab_reactivate_ = false;

  // When the Immersive Reading Mode overlay is shown, it covers the main web
  // contents, changing it's visibility to Visibility::OCCLUDED. This causes
  // the renderer to make optimizations that break Reading Mode (namely, that it
  // can stop generating accessibility events). This method tells the renderer
  // that even though the webpage is technically occluded, we want it treated as
  // if it were visible.
  void CaptureMainContentsAsVisible();
  // Reset the main contents capturer handle_ when we no longer need to force
  // the main webpage to be treated as visible for IRM purposes.
  void ReleaseMainContentsCapture();

  DistillationState distillation_state_ = DistillationState::kUndefined;
  bool distillation_state_locked_for_testing_ = false;

  // The handle returned by web_contents_->IncrementCapturerCount. This is used
  // to release the capture when the ReadAnythingController is destroyed.
  // Note: Do not access this directly. Use CaptureMainContentsAsVisible() and
  // ReleaseMainContentsCapture() instead to ensure the handle is correctly
  // managed.
  base::ScopedClosureRunner main_contents_capturer_handle_;

  raw_ptr<ReadAnythingService> active_service_ = nullptr;

  static bool freeze_distillation_for_testing_;

  base::WeakPtrFactory<ReadAnythingController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_
