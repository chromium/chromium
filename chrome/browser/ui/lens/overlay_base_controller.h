// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_OVERLAY_BASE_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_OVERLAY_BASE_CONTROLLER_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/lens/lens_overlay_blur_layer_delegate.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class WebView;
}  // namespace views

class PrefService;

// Manages all state associated with the overlay.
// This class is not thread safe. It should only be used from the browser
// thread.
class OverlayBaseController : public content::WebContentsDelegate,
                              public views::ViewObserver,
                              public views::WidgetObserver,
                              public content::RenderProcessHostObserver,
                              public ImmersiveModeController::Observer,
                              public content::WebContentsObserver {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOverlayId);

  OverlayBaseController(tabs::TabInterface* tab, PrefService* pref_service);
  ~OverlayBaseController() override;

  // Internal state machine. States are mutually exclusive. Exposed for testing.
  enum class State {
    // This is the default state. There should be no performance overhead as
    // this state will apply to all tabs.
    kOff,

    // Waiting for reflow after closing side panel before taking a full page
    // screenshot.
    kClosingOpenedSidePanel,

    // In the process of taking a screenshot to transition to kOverlay.
    kScreenshot,

    // In the process of starting the overlay WebUI.
    kStartingWebUI,

    // Showing an overlay without results.
    kOverlay,

    // The UI is hidden, but the session is still active (e.g. side panel
    // is showing results). This differs from kBackground, where the tab is
    // inactive.
    kHidden,

    // The UI has been made inactive because the tab has been backgrounded.
    // The overlay and web view are not freed and could be
    // immediately reshown.
    kBackground,

    // Will be kOff soon.
    kClosing,

    // The UI is in the process of being shown after being hidden. Will
    // transition to kOverlay unless interrupted by the overlay becoming
    // hidden from a tab switch or other similar process. In these cases, the
    // overlay will transition to kHidden and will need to be reshown again.
    kIsReshowing,

    // In the process of fading out before being completely hidden. Will
    // transition to kHidden.
    kHiding,
  };
  State state() { return state_; }

  // When a tab is in the background, the WebContents may be discarded to save
  // memory. When a tab is in the foreground it is guaranteed to have a
  // WebContents.
  const content::WebContents* tab_contents() { return tab_->GetContents(); }

  State backgrounded_state_for_testing() { return backgrounded_state_; }

  views::Widget* get_preselection_widget_for_testing() {
    return preselection_widget_.get();
  }

  // Returns true if the overlay is open and covering the current active tab.
  bool IsOverlayShowing() const;

  // Returns true if the overlay is showing or is in live page mode.
  bool IsOverlayActive() const;

  // Returns true if the overlay is in the process of initializing.
  bool IsOverlayInitializing();

  // Returns true if the overlay is currently in the process of closing.
  bool IsOverlayClosing();

  // Returns the tab interface that that owns the search controller that owns
  // this overlay controller.
  tabs::TabInterface* GetTabInterface();

  // Called when the associated tab enters the foreground.
  void TabForegrounded(tabs::TabInterface* tab);

  // Called when the associated tab will enter the background.
  void TabWillEnterBackground(tabs::TabInterface* tab);

  // Testing helper method for checking the blur layer delegate.
  lens::LensOverlayBlurLayerDelegate*
  GetLensOverlayBlurLayerDelegateForTesting();

  // Testing helper method for checking view housing our overlay.
  views::View* GetOverlayViewForTesting();

  // Testing helper method for checking web view.
  views::WebView* GetOverlayWebViewForTesting();

 private:
  // ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  // views::WidgetObserver:
#if BUILDFLAG(IS_MAC)
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
#endif
  void OnWidgetDestroying(views::Widget* widget) override;

  // ImmersiveModeController::Observer:
  void OnImmersiveRevealStarted() override;
  void OnImmersiveRevealEnded() override;
  void OnImmersiveFullscreenEntered() override;
  void OnImmersiveFullscreenExited() override;

  // content::RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;

  // Called when the UI needs to create the view to show in the overlay.
  raw_ptr<views::View> CreateViewForOverlay();

  // Called when the side panel alignment chgces.
  void OnSidePanelAlignmentChanged();

  // Called anytime the side panel opens. Used to close lens overlay when
  // another side panel opens.
  void OnSidePanelDidOpen();

  // The screenshot is ready for reshow, continue showing the overlay.
  void ReshowScreenshotReady(base::OnceCallback<void(SkBitmap)> callback,
                             SkBitmap rgb_screenshot);

  // Sets the opacity of the overlay web view. No-op if the web view does not
  // exist.
  void SetOverlayWebViewOpacity(float opacity);

 protected:
  // Whether the side panel is showing.
  virtual bool IsResultsSidePanelShowing() = 0;

  // Returns the initial WebUI URL.
  virtual GURL GetInitialURL() = 0;

  enum class DismissalSource {
    kPreselectionToastExitButton,
    kPreselectionToastEscapeKeyPress,
    kErrorScreenshotCreationFailed,
    kOverlayRendererClosedNormally,
    kOverlayRendererClosedUnexpectedly,
    kUnexpectedSidePanelOpen,
    kPageRendererClosedNormally,
    kPageRendererClosedUnexpectedly
  };

  // Request synchronous close of the overlay.
  virtual void RequestSyncClose(DismissalSource source) = 0;

  // Notifies whether the overlay is showing.
  virtual void NotifyIsOverlayShowing(bool is_showing) = 0;

  // Returns the resource id for this overlay.
  virtual int GetToolResourceId() = 0;

  // Return the ID of the view we attach this overlay to.
  virtual ui::ElementIdentifier GetViewContainerId() = 0;

  // The side panel type.
  virtual SidePanelEntry::PanelType GetSidePanelType() = 0;

  // Whether the side panel should be closed if it doesn't match
  // the desired type.
  virtual bool ShouldCloseSidePanel() = 0;

  // Start the flow for collecting the screenshot. After the concrete
  // class has prepared the screenshot `InitializeScreenshot` should be called.
  virtual void StartScreenshotFlow() = 0;

  // Whether the presselection bubble should be shown.
  virtual bool ShouldShowPreselectionBubble() = 0;

  // Whether we should blur the host view.
  virtual bool UseOverlayBlur() = 0;

  // Notify the page was navigated.
  virtual void NotifyPageNavigated() = 0;

  // Notification that the overlay is closing soon.
  virtual void NotifyOverlayClosing() = 0;

  // Notification that the tab was foregrounded.
  virtual void NotifyTabForegrounded() = 0;

  // Notification that the tab was foregrounded.
  virtual void NotifyTabWillEnterBackground() = 0;

  // Returns if the overlay view can be shared between multiple tabs.
  virtual bool IsOverlayViewShared() const = 0;

  // If the side panel was closed, we wait for the reflow before beginning
  // the screenshot flow.
  virtual void FinishedWaitingForReflow(base::TimeTicks reflow_start_time);

  // Whether it's possible to capture a screenshot. virtual for testing.
  virtual bool IsScreenshotPossible(content::RenderWidgetHostView* view);

  // Close the UI.
  virtual void CloseUI();

  // Reshow the overlay using the current viewport screenshot and page context
  // on the live page. The callee should call `ReshowScreenshot` when the
  // screenshot is ready.
  virtual void ReshowOverlay();

  // This is callwed when the webUI acknowledges the intent to reshow the
  // overlay. Since it already is showing an old screenshot the opacity is set
  // to 0 of the layer when the screenshot is first available to the webUI.
  // After it has produced a frame it should call this FinishReshowOverlayImpl
  // method to change the opacity back to 1.
  void FinishReshowOverlayImpl();

  // Plays the overlay close animation and then invokes the callback.
  void TriggerOverlayFadeOutAnimation(base::OnceClosure callback);

  // Process the bitmap and creates all necessary data to initialize the
  // overlay. Happens on a separate thread to prevent main thread from hanging.
  // Callback is called after creating the RGB bitmap and we are back on the
  // main thread.
  void InitializeScreenshot(const SkBitmap& screenshot,
                            base::OnceCallback<void(SkBitmap)> callback);

  // Similar to `InitializeScreenshot` this prepares
  // the screenshot for the webUI. However this is only called when the UI is in
  // reshowing state and since the WebUI is rendered it needs to be careful not
  // to cause a flash.
  void ReshowScreenshot(const SkBitmap& screenshot,
                        base::OnceCallback<void(SkBitmap)> callback);

  // Initialize the overlay.
  void InitializeOverlayImpl();

  // Enable/Disable live blur.
  void SetLiveBlurImpl(bool enabled);

  // Add the background blur delegate to the layer hierarchy.
  void AddBackgroundBlurImpl();

  // Sets the top right or top left corner of the overlay to be rounded if the
  // side panel is open and the `SideBySide` feature is enabled. This is
  // necessary because rounded corners are owned by the `MultiContentsView`,
  // and the overlay is shown on top of it.
  // TODO(crbug.com/443102583): Remove this block if `overlay_view_` ends up
  // getting reparented into `MultiContentsView`.
  void SetOverlayRoundedCorner();

  // Whether we can show modal UI now.
  bool CanShowModalUI();

  // Start the showing the modal UI. This requires a number of asynchronous
  // steps, capturing the screenshot, converting the screenshot and starting the
  // webUI.
  void ShowModalUI();

  // Called when the UI needs to show the overlay via a view that is a child of
  // the tab contents view.
  void ShowOverlay();

  // Hides the overlay view and restores input to the tab contents web view.
  // This does not change any overlay state.
  void HideOverlay();

  // Hides the overlay, but also sets the state to kHidden.
  void HideOverlayAndSetHiddenState();

  // Hide the shared overlay view if it is not being used by another tab. This
  // is determined by checking if any of the children of the overlay view are
  // visible.
  void MaybeHideSharedOverlayView();

  // Show preselection toast bubble. Creates a preselection bubble if it does
  // not exist.
  void ShowPreselectionBubble();

  // Closes the preselection bubble and reopens it. Used to prevent UI conflicts
  // between the preselection bubble and top chrome in fullscreen.
  void CloseAndReshowPreselectionBubble();

  // Hides preselection toast bubble. Used when backgrounding the overlay. This
  // hides the widget associated with the bubble.
  void HidePreselectionBubble();

  // Close the preselection bubble.
  void ClosePreselectionBubbleImpl();

  // content::WebContentsObserver:
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Owns the this class via TabFeatures.
  raw_ptr<tabs::TabInterface> tab_;

  // Tracks the internal state machine.
  State state_ = State::kOff;

  // Tracks the state of the overlay when it is backgrounded. This is the state
  // that the overlay will return to when the tab is foregrounded.
  State backgrounded_state_ = State::kOff;

  // The pref service associated with the current profile.
  raw_ptr<PrefService> pref_service_;

 private:
  // Prevents other features from showing tab-modal UI.
  std::unique_ptr<tabs::ScopedTabModalUI> scoped_tab_modal_ui_;

  // Indicates whether live blur should be enabled when the overlay is shown.
  bool should_enable_live_blur_on_show_ = false;

  // ---------------Browser window scoped state: START---------------------
  // State that is scoped to the browser window must be reset when the tab is
  // backgrounded, since the tab may move between browser windows.

  // Observes the side panel of the browser window.
  base::CallbackListSubscription side_panel_shown_subscription_;

  // Observer to check when the content web view bounds change.
  base::ScopedObservation<views::View, views::ViewObserver>
      tab_contents_view_observer_{this};

  // Observer to check when the preselection widget is deleted.
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      preselection_widget_observer_{this};

  // Observer to get notifications when the immersive mode reveal state changes.
  base::ScopedObservation<ImmersiveModeController,
                          ImmersiveModeController::Observer>
      immersive_mode_observer_{this};

  // Layer delegate that handles blurring the background behind the WebUI.
  std::unique_ptr<lens::LensOverlayBlurLayerDelegate>
      overlay_blur_layer_delegate_;

 protected:
  // Pointer to the view that houses our overlay as a child of the tab
  // contents web view.
  raw_ptr<views::View> overlay_view_;

  // Pointer to the web view within the overlay view if it exists.
  raw_ptr<views::WebView> overlay_web_view_;

  // Preselection toast bubble. Weak; owns itself. NULL when closed.
  raw_ptr<views::Widget> preselection_widget_ = nullptr;

 private:
  // The anchor view to the preselection bubble. This anchor is an invisible
  // sibling of the the `overlay_view_`, user to always keep the preselection
  // bubble anchored to the top of the screen, while also maintaining focus
  // order.
  raw_ptr<views::View> preselection_widget_anchor_;

  // Register for adding observers to prefs the current profiles pref service.
  // Used to observe the immersive mode pref on Mac, and the side panel
  // horizontal alignment pref.
  PrefChangeRegistrar pref_change_registrar_;

  // --------------------Browser window scoped state: END---------------------
  // Must be the last member.
  base::WeakPtrFactory<OverlayBaseController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_LENS_OVERLAY_BASE_CONTROLLER_H_
