// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_SIDE_PANEL_COORDINATOR_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/simple_menu_model.h"

class GURL;
class LensOverlayController;
class LensOverlaySidePanelWebView;

enum class SidePanelEntryHideReason;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
}  // namespace views

namespace lens {

// Handles the creation and registration of the lens overlay side panel entry.
// There are two ways for this instance to be torn down.
//   (1) Its owner, LensOverlayController can destroy it.
//   (2) `side_panel_web_view_` can be destroyed by the side panel.
// In the case of (2), this instance is no longer functional and needs to be
// torn down. There are two constraints:
//   (2a) The shutdown path of LensOverlayController must be asynchronous. This
//   avoids re-entrancy into the code that is in turn calling (2).
//   (2b) Clearing local state associated with `side_panel_web_view_` must be
//   done synchronously.
class LensOverlaySidePanelCoordinator
    : public SidePanelEntryObserver,
      public content::WebContentsObserver,
      public ChromeWebModalDialogManagerDelegate,
      public ui::SimpleMenuModel::Delegate {
 public:
  explicit LensOverlaySidePanelCoordinator(
      LensOverlayController* lens_overlay_controller);
  LensOverlaySidePanelCoordinator(const LensOverlaySidePanelCoordinator&) =
      delete;
  LensOverlaySidePanelCoordinator& operator=(
      const LensOverlaySidePanelCoordinator&) = delete;
  ~LensOverlaySidePanelCoordinator() override;

  // Registers the side panel entry in the side panel if it doesn't already
  // exist and then shows it.
  void RegisterEntryAndShow();

  // SidePanelEntryObserver:
  void OnEntryWillHide(SidePanelEntry* entry,
                       SidePanelEntryHideReason reason) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

  // Called by the destructor of the side panel web view.
  void WebViewClosing();

  content::WebContents* GetSidePanelWebContents();

  // Return the LensOverlayController that owns this side panel coordinator.
  LensOverlayController* GetLensOverlayController() {
    return lens_overlay_controller_.get();
  }

  // Whether the lens overlay entry is currently the active entry in the side
  // panel UI.
  bool IsEntryShowing();

  enum CommandID {
    COMMAND_MY_ACTIVITY,
    COMMAND_LEARN_MORE,
    COMMAND_SEND_FEEDBACK,
  };

  // SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // content::WebContentsObserver:
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;

  // ChromeWebModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // Opens the provided url params in the main browser as a new tab.
  void OpenURLInBrowser(const content::OpenURLParams& params);

  // Registers the entry in the side panel if it doesn't already exist.
  void RegisterEntry();

  // Called to get the URL for the "open in new tab" button.
  GURL GetOpenInNewTabUrl();

  std::unique_ptr<views::View> CreateLensOverlayResultsView();

  // Returns the more info callback for creating the side panel entry.
  base::RepeatingCallback<std::unique_ptr<ui::MenuModel>()>
  GetMoreInfoCallback();

  // Returns the menu model for the more info menu.
  std::unique_ptr<ui::MenuModel> GetMoreInfoMenuModel();

  // Owns this.
  const raw_ptr<LensOverlayController> lens_overlay_controller_;

  raw_ptr<LensOverlaySidePanelWebView> side_panel_web_view_;
  base::WeakPtrFactory<LensOverlaySidePanelCoordinator> weak_ptr_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_SIDE_PANEL_COORDINATOR_H_
