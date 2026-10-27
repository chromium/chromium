// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/read_anything/read_anything_contents_wrapper.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_immersive_activation_observer.h"
#include "chrome/browser/ui/read_anything/read_anything_lifecycle_observer.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "chrome/common/read_anything/read_anything.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "pdf/buildflags.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class NavigationHandle;
struct OpenURLParams;
}  // namespace content

namespace input {
struct NativeWebKeyboardEvent;
}  // namespace input

namespace read_anything {
class ReadAnythingEntryPointController;
}  // namespace read_anything

class ReadAnythingController;
class ReadAnythingHatsSurveyController;
class ReadAnythingOmniboxController;
class ReadAnythingSidePanelController;
class ReadAnythingUntrustedPageHandler;
class SidePanelRegistry;

#if BUILDFLAG(ENABLE_PDF)
class GURL;

// Returns `pdf_url` with the `view` open-PDF parameter in its fragment set to
// "FitH" (fit to width). Any existing `view` parameters are replaced by a
// single one at the end; all other fragment parameters are preserved. Exposed
// for testing.
GURL GetPdfUrlWithFitToWidth(const GURL& pdf_url);
#endif  // BUILDFLAG(ENABLE_PDF)

// A helper class to observe a specific WebContents, so the ReadAnything
// Controller can observe multiple WebContents. Event callbacks are configured
// by the instantiator.
class WebContentsObserverInstance : public content::WebContentsObserver {
 public:
  WebContentsObserverInstance(
      content::WebContents* web_contents,
      base::RepeatingClosure renderer_crashed_callback,
      base::RepeatingCallback<void(content::Visibility)>
          visibility_changed_callback);

  WebContentsObserverInstance(const WebContentsObserverInstance&) = delete;
  WebContentsObserverInstance& operator=(const WebContentsObserverInstance&) =
      delete;
  ~WebContentsObserverInstance() override;

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void OnRendererUnresponsive(
      content::RenderProcessHost* render_process_host) override;

 private:
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
// External callers must use ReadAnythingEntryPointController to open or toggle
// Reading Mode. ReadAnythingController orchestrates the display and lifecycle
// of the Reading Mode UI once invoked. Once Reading Mode is open, controls
// inside its own WebUI (through ReadAnythingUntrustedPageHandler) can also
// close the immersive overlay or switch presentations.
class ReadAnythingController : public tabs::ContentsObservingTabFeature {
 public:
  using Observer = ReadAnythingLifecycleObserver;

  ReadAnythingController(const ReadAnythingController&) = delete;
  ReadAnythingController& operator=(const ReadAnythingController&) = delete;
  ~ReadAnythingController() override;

  using PresentationState = read_anything::mojom::ReadAnythingPresentationState;

  using DistillationState = read_anything::mojom::ReadAnythingDistillationState;

  using ReadAnythingOpenTrigger = read_anything::mojom::ReadAnythingOpenTrigger;

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
  void OnEntryShown(ReadAnythingOpenTrigger trigger);
  void OnEntryHidden();

  // Shows Reading Mode using the user's preferred presentation mode.
  void ShowInPreferredUI(
      base::PassKey<read_anything::ReadAnythingEntryPointController>,
      ReadAnythingOpenTrigger trigger);

  // Shows Reading Mode in the side panel, moving it out of the immersive
  // overlay if it's showing there.
  void ShowInSidePanel(
      base::PassKey<read_anything::ReadAnythingEntryPointController>,
      ReadAnythingOpenTrigger trigger);

  // Closes Reading Mode in whichever presentation it's showing in.
  void CloseUI(base::PassKey<read_anything::ReadAnythingEntryPointController>,
               ReadAnythingCloseReason reason);

  // Toggles the Reading Mode UI.
  void ToggleUI(base::PassKey<read_anything::ReadAnythingEntryPointController>,
                ReadAnythingOpenTrigger trigger);

  // The following methods back controls inside Reading Mode's own WebUI, so
  // they're restricted to ReadAnythingUntrustedPageHandler. They act on Reading
  // Mode once it's open, rather than being entry points that open it, so they
  // don't go through ReadAnythingEntryPointController.

  // Closes the Immersive Reading Mode UI. Backs the immersive overlay's close
  // button.
  void CloseImmersiveUI(base::PassKey<ReadAnythingUntrustedPageHandler>,
                        ReadAnythingCloseReason reason);

  // Toggles between the Immersive Reading Mode UI and the Side Panel UI. Backs
  // Reading Mode's presentation menu.
  void TogglePresentation(base::PassKey<ReadAnythingUntrustedPageHandler>,
                          bool is_user_initiated);

  // Returns the current presentation_state_ of the Reading Mode feature. This
  // refers to the current host of the WebUI, but does not guarantee that the
  // feature is necessarily showing by the host.
  PresentationState GetPresentationState() const;

  // Handles OpenURL requests originating from Reading Mode WebUI instances
  // (both side panel and immersive views). Enforces security policies
  // (restricting schemes to web schemes, validating transient user activation,
  // and demoting dangerous dispositions) before delegating to the main tab's
  // WebContentsDelegate.
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback);

  // Handles Escape key press to exit fullscreen via ExclusiveAccessManager.
  // Returns true if the event was handled.
  bool HandleEscapeKey(const input::NativeWebKeyboardEvent& event);

  void OnDistillationStateChanged(DistillationState new_state);

  // Called when the Reading Mode WebUI reports the result of a distillation
  // attempt. Coordinates any follow-up work that depends on how Reading Mode
  // was opened (e.g. logging omnibox entry point metrics).
  void OnDistillationStatus(read_anything::mojom::DistillationStatus status,
                            int word_count);

  static constexpr int kMaxWordsDistilled = 25000;
  static constexpr int kWordsDistilledBuckets = 100;

  // For testing only. Pauses this controller's reactions to distillation state
  // changes.
  static void SetFreezeDistillationOnCreationForTesting(bool locked);

  // Lazily creates and returns the WebUIContentsWrapper for the
  // Reading Mode WebUI. Transfers ownership of the WebUIContentsWrapper to the
  // caller, and the caller passes in the presentation that the webui will be
  // presented in (e.g. kInSidePanel, kInImmersiveOverlay)
  ReadAnythingContentsWrapper GetOrCreateWebUIWrapper(
      PresentationState web_ui_new_presentation_state);

  // Getter for has_shown_ui_. This is used by RM host views to
  // determine if the Reading Mode is ready to be shown, or if it should wait
  // for a notification that it is ready.
  bool has_shown_ui() const { return has_shown_ui_; }

  // Returns whether Reading Mode is switching between the side panel and
  // immersive presentations. True from the start of a switch until the new
  // presentation is shown.
  bool is_presentation_transitioning() const {
    return is_presentation_transitioning_;
  }

  void SetWebUIWrapperForTest(ReadAnythingContentsWrapper web_ui_wrapper);

  // Called by other host views of the Reading Mode WebUI to return ownership of
  // the WebUIContentsWrapper to this controller.
  void TransferWebUiOwnership(ReadAnythingContentsWrapper web_ui_wrapper,
                              PresentationState from_presentation);

  // Recreates the WebUI on the next GetOrCreateWebUIWrapper() call. This should
  // be called if Reading mode crashes so that we don't get stuck in a crashed
  // state.
  void RecreateWebUIWrapper();

  // Artificially sets the time when the user entered a page for testing the
  // omnibox entry point.
  void SetDwellTimeForTesting(base::TimeTicks test_time);

  void OnSoftNavigation();

  // Called when the Reading Mode side panel entry is about to be hidden.
  void OnSidePanelWillHide(SidePanelEntryHideReason side_panel_reason);

 private:
  friend class ReadAnythingControllerBrowserTest;
  friend class ReadAnythingControllerUnitTest;
  friend class ReadAnythingIphControllerTest;

  // Displays the Reading Mode UI in the Side Panel.
  void ShowSidePanelUI(SidePanelOpenTrigger trigger);

  // Displays the Immersive Reading Mode UI in a full screen overlay.
  void ShowImmersiveUI(ReadAnythingOpenTrigger trigger);

  // Closes the Immersive Reading Mode UI.
  void CloseImmersiveUI(ReadAnythingCloseReason reason);

  // Closes the Reading mode side panel UI.
  void CloseSidePanelUI(ReadAnythingCloseReason reason);

  // Shows Reading Mode using the user's preferred presentation mode.
  void ShowInPreferredUI(ReadAnythingOpenTrigger trigger);

  // Toggles the Reading Mode UI.
  void ToggleUI(ReadAnythingOpenTrigger trigger);

  // Toggles between the Immersive Reading Mode UI and the Side Panel UI.
  void TogglePresentation(bool is_user_initiated);

  void SetPresentationState(PresentationState new_state);

  // Saves the presentation state to the user's preferences.
  void SavePresentationPreference(PresentationState state);

  // Updates the FindBarController's target WebContents if necessary. This
  // ensures that find-in-page (Cmd-F) targets the IRM overlay when it's open,
  // rather than the occluded main WebContents.
  void MaybeUpdateFindBarController();

  std::unique_ptr<WebContentsObserverInstance> ra_web_ui_observer_;
  std::unique_ptr<ReadAnythingOmniboxController> omnibox_controller_;
  std::unique_ptr<ReadAnythingHatsSurveyController> hats_survey_;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  // Callback for when ra_web_ui_observer_ receives a OnVisibilityChanged
  // event.
  void OnReadAnythingVisibilityChanged(content::Visibility visibility);

  // Callback for when ra_web_ui_observer_ determines the renderer has crashed
  // (e.g. due to being unresponsive).
  void OnRendererCrashed();

  // Records OnEntryHidden metrics and returns the calculated session duration
  // if Reading Mode was successfully shown. Returns std::nullopt if the UI is
  // closed before being successfully shown or hidden due to an in-progress
  // presentation mode transition.
  std::optional<base::TimeDelta> RecordEntryHiddenMetrics();

  // Returns the SidePanelUI for the active tab if it can be shown.
  // Otherwise, returns nullptr.
  SidePanelUI* GetSidePanelUI();

  raw_ptr<tabs::TabInterface> tab_ = nullptr;
  ui::ScopedUnownedUserData<ReadAnythingController> scoped_unowned_user_data_;

  ReadAnythingContentsWrapper web_ui_wrapper_;

  std::unique_ptr<ReadAnythingSidePanelController>
      read_anything_side_panel_controller_;

  bool has_shown_ui_ = false;
  bool should_recreate_web_ui_ = false;

  // Timestamp of when the Reading Mode overlay begins to be shown.
  base::TimeTicks entry_shown_timestamp_;

  // Returns if we are in the process of switching the Reading Mode Overlay.
  // Used to ensure we keep transitions between RM UI states ( Immersive -> SP
  // and vice-versa) as part of the same RM session.
  bool is_presentation_transitioning_ = false;

  // Stores the specific reason why this controller is closing the side panel
  // programmatically (e.g. renderer crash or presentation mode switch).
  // This ensures observers receive the accurate ReadAnythingCloseReason when
  // OnSidePanelWillHide() is invoked. If this value is empty at that time, then
  // the side panel was closed a different way, determined by the
  // SidePanelEntryHideReason.
  std::optional<ReadAnythingCloseReason> pending_side_panel_close_reason_;

  // TODO(crbug.com/484371187): Investigate if reentrancy can be removed.
  base::ObserverList<
      Observer,
      /*check_empty=*/false,
      base::ObserverListReentrancyPolicy::kAllowReentrancyUntriaged>
      observers_;

  base::ObserverList<ReadAnythingImmersiveActivationObserver>
      immersive_activation_observers_;

  // Holds subscription for TabInterface callbacks.
  base::CallbackListSubscription tab_did_activate_subscription_;

  PresentationState presentation_state_ = PresentationState::kUndefined;
  // When a tab becomes inactive and hides IRM, this is set to true to let us
  // know we should show IRM again when the tab becomes active again.
  bool should_show_immersive_on_tab_reactivate_ = false;

  // When the Immersive Reading Mode overlay is shown, it covers the main web
  // contents, changing it's visibility to Visibility::OCCLUDED. This causes
  // the renderer to make optimizations that break Reading Mode (namely, that
  // it can stop generating accessibility events). This method tells the
  // renderer that even though the webpage is technically occluded, we want it
  // treated as if it were visible.
  void CaptureMainContentsAsVisible();
  // Reset the main contents capturer handle_ when we no longer need to force
  // the main webpage to be treated as visible for IRM purposes.
  void ReleaseMainContentsCapture();

  DistillationState distillation_state_ = DistillationState::kUndefined;

  // The trigger used the last time Reading Mode was shown. Intentionally not
  // reset when Reading Mode is hidden, so that a distillation result arriving
  // after the UI is closed is still attributed to the right entry point.
  // OnDistillationStatus() resets it to kUnknown after logging the omnibox
  // distillation metrics, so they are only recorded once per omnibox open.
  ReadAnythingOpenTrigger last_open_trigger_ =
      ReadAnythingOpenTrigger::kUnknown;

  // The handle returned by web_contents_->IncrementCapturerCount. This is
  // used to release the capture when the ReadAnythingController is destroyed.
  // Note: Do not access this directly. Use CaptureMainContentsAsVisible() and
  // ReleaseMainContentsCapture() instead to ensure the handle is correctly
  // managed.
  base::ScopedClosureRunner main_contents_capturer_handle_;

  static bool freeze_distillation_for_testing_;
};

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_
