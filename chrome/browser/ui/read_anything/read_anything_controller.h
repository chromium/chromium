// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class TabStripModel;

// Manages the core logic for the Reading Mode feature.
//
// This controller is owned by TabFeatures and is instantiated once per tab.
// Its lifetime is tied to the TabInterface.
//
// It acts as the primary entry point for all Reading Mode commands and is
// responsible for orchestrating the display of the Reading Mode UI.
class ReadAnythingController : public TabStripModelObserver,
                               content::WebContentsObserver {
 public:
  ReadAnythingController(const ReadAnythingController&) = delete;
  ReadAnythingController& operator=(const ReadAnythingController&) = delete;
  ~ReadAnythingController() override;

  enum class PresentationState {
    kInactive,
    kInSidePanel,
    kInImmersiveOverlay,
  };

  explicit ReadAnythingController(tabs::TabInterface* tab);

  DECLARE_USER_DATA(ReadAnythingController);
  static ReadAnythingController* From(tabs::TabInterface* tab);

  // Displays the Reading Mode UI by utilizing the SidePanelUI on the active
  // tab.
  // TODO(crbug.com/447418049): Open immersive reading mode via this entrypoint.
  void ShowUI(SidePanelOpenTrigger trigger);

  // Toggles the Reading Mode UI by utilizing the SidePanelUI on the active
  // tab.
  // TODO(crbug.com/447418049): Open immersive reading mode via this entrypoint.
  void ToggleReadAnythingSidePanel(SidePanelOpenTrigger trigger);

  // Returns the current presentation state of the Reading Mode feature.
  PresentationState GetPresentationState() const;

  // Lazily creates and returns the WebUIContentsWrapper for the
  // Reading Mode WebUI. Transfers ownership of the WebUIContentsWrapper to the
  // caller.
  std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
  GetOrCreateWebUIWrapper();

  // Getter for has_shown_ui_. This is used by RM host views to
  // determine if the Reading Mode is ready to be shown, or if it should wait
  // for a notification that it is ready.
  bool has_shown_ui() const { return has_shown_ui_; }

  void SetWebUIWrapperForTest(
      std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
          web_ui_wrapper);

  // Test function while OnTabStripModelChanged events are implemented.
  // TODO(crbug.com/463732840): Remove this function once the
  // OnTabStripModelChanged events are implemented.
  bool isActiveTab();

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection_change) override;

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;

  // Called by other host views of the Reading Mode WebUI to return ownership of
  // the WebUIContentsWrapper to this controller.
  void TransferWebUiOwnership(
      std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
          web_ui_wrapper);

 private:
  // Returns the SidePanelUI for the active tab if it can be shown.
  // Otherwise, returns nullptr.
  SidePanelUI* GetSidePanelUI();

  raw_ptr<tabs::TabInterface> tab_ = nullptr;
  ui::ScopedUnownedUserData<ReadAnythingController> scoped_unowned_user_data_;

  std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
      web_ui_wrapper_;

  // Keeps track of whether the tab is active.
  // TODO(crbug.com/463732840): Detemrine if this variable is needed once the
  // OnTabStripModelChanged events are implemented.
  bool is_active_tab_ = false;

  bool has_shown_ui_ = false;
};

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_
