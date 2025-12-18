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
#include "chrome/browser/ui/read_anything/read_anything_lifecycle_observer.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/common/read_anything/read_anything.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class ReadAnythingController;
class TabStripModel;

// A helper class to observe a specific WebContents, so the ReadAnything
// Controller can observe multiple WebContents. Event callbacks are configured
// by the instantiator.
class WebContentsObserverInstance : public content::WebContentsObserver {
 public:
  WebContentsObserverInstance(
      content::WebContents* web_contents,
      base::RepeatingClosure primary_page_changed_callback,
      base::RepeatingCallback<void(content::Visibility)>
          visibility_changed_callback);

  WebContentsObserverInstance(const WebContentsObserverInstance&) = delete;
  WebContentsObserverInstance& operator=(const WebContentsObserverInstance&) =
      delete;
  ~WebContentsObserverInstance() override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  base::RepeatingClosure primary_page_changed_callback_;
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
class ReadAnythingController : public TabStripModelObserver {
 public:
  using Observer = ReadAnythingLifecycleObserver;

  ReadAnythingController(const ReadAnythingController&) = delete;
  ReadAnythingController& operator=(const ReadAnythingController&) = delete;
  ~ReadAnythingController() override;

  using PresentationState = read_anything::mojom::ReadAnythingPresentationState;

  ReadAnythingController(tabs::TabInterface* tab,
                         SidePanelRegistry* side_panel_registry);

  DECLARE_USER_DATA(ReadAnythingController);
  static ReadAnythingController* From(tabs::TabInterface* tab);

  tabs::TabInterface* tab() { return tab_.get(); }

  // Adds/removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Called by ReadAnythingSidePanelController when the WebUI is
  // shown/hidden.
  void OnEntryShown(std::optional<ReadAnythingOpenTrigger> trigger);
  void OnEntryHidden();

  // Displays the Reading Mode UI by utilizing the SidePanelUI on the active
  // tab.
  // TODO(crbug.com/447418049): Open immersive reading mode via this entrypoint.
  void ShowUI(SidePanelOpenTrigger trigger);

  // Displays the Immersive Reading Mode UI in a full screen overlay.
  void ShowImmersiveUI(ReadAnythingOpenTrigger trigger);

  // Closes the Immersive Reading Mode UI.
  void CloseImmersiveUI(bool closed_by_tab_switch = false);

  // Toggles the Reading Mode UI by utilizing the SidePanelUI on the active
  // tab.
  // TODO(crbug.com/447418049): Open immersive reading mode via this entrypoint.
  void ToggleReadAnythingSidePanel(SidePanelOpenTrigger trigger);

  int GetNavCounterForTesting() const;

  // Returns the current presentation_state_ of the Reading Mode feature. This
  // refers to the current host of the WebUI, but does not guarantee that the
  // feature is necessarily showing by the host.
  PresentationState GetPresentationState() const;

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


  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection_change) override;

  // Called by other host views of the Reading Mode WebUI to return ownership of
  // the WebUIContentsWrapper to this controller.
  void TransferWebUiOwnership(
      std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
          web_ui_wrapper);

 private:
  // Called when the tab will detach.
  void TabWillDetach(tabs::TabInterface* tab,
                     tabs::TabInterface::DetachReason reason);
  // Called when the tab is activated.
  void OnTabActivated();
  // Called when the tab is backgrounded.
  void OnTabBackgrounded();

  std::unique_ptr<WebContentsObserverInstance> main_page_observer_;
  std::unique_ptr<WebContentsObserverInstance> ra_web_ui_observer_;

  // Callback for when main_page_observer_ receives a PrimaryPageChanged event.
  void OnMainPagePrimaryPageChanged();

  // Callback for when ra_web_ui_observer_ receives a OnVisibilityChanged
  // event.
  void OnReadAnythingVisibilityChanged(content::Visibility visibility);

  // Returns the SidePanelUI for the active tab if it can be shown.
  // Otherwise, returns nullptr.
  SidePanelUI* GetSidePanelUI();

  // TODO(crbug.com/460136558): Used for tests, remove when implementing
  // OnTabNavigation.
  int nav_counter_ = 0;

  raw_ptr<tabs::TabInterface> tab_ = nullptr;
  ui::ScopedUnownedUserData<ReadAnythingController> scoped_unowned_user_data_;

  std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
      web_ui_wrapper_;

  std::unique_ptr<ReadAnythingSidePanelController>
      read_anything_side_panel_controller_;

  bool has_shown_ui_ = false;

  base::ObserverList<Observer> observers_;

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
  // The handle returned by web_contents_->IncrementCapturerCount. This is used
  // to release the capture when the ReadAnythingController is destroyed.
  // Note: Do not access this directly. Use CaptureMainContentsAsVisible() and
  // ReleaseMainContentsCapture() instead to ensure the handle is correctly
  // managed.
  base::ScopedClosureRunner main_contents_capturer_handle_;

  base::WeakPtrFactory<ReadAnythingController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_
