// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_controller.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_entry_point_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_omnibox_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/read_anything/read_anything_service.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/views/accessibility/view_accessibility.h"

///////////////////////////////////////////////////////////////////////////////
// WebContentsObserverInstance

WebContentsObserverInstance::WebContentsObserverInstance(
    content::WebContents* web_contents,
    base::RepeatingClosure primary_page_changed_callback,
    base::RepeatingClosure renderer_crashed_callback,
    base::RepeatingCallback<void(content::Visibility)>
        visibility_changed_callback)
    : content::WebContentsObserver(web_contents),
      primary_page_changed_callback_(primary_page_changed_callback),
      renderer_crashed_callback_(renderer_crashed_callback),
      visibility_changed_callback_(visibility_changed_callback) {}

WebContentsObserverInstance::~WebContentsObserverInstance() = default;

// content::WebContentsObserver:
void WebContentsObserverInstance::PrimaryPageChanged(content::Page& page) {
  primary_page_changed_callback_.Run();
}

void WebContentsObserverInstance::OnVisibilityChanged(
    content::Visibility visibility) {
  visibility_changed_callback_.Run(visibility);
}

void WebContentsObserverInstance::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  renderer_crashed_callback_.Run();
}

void WebContentsObserverInstance::OnRendererUnresponsive(
    content::RenderProcessHost* render_process_host) {
  renderer_crashed_callback_.Run();
}

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingController

DEFINE_USER_DATA(ReadAnythingController);

WEB_CONTENTS_USER_DATA_KEY_IMPL(ReadAnythingControllerGlue);

ReadAnythingControllerGlue::ReadAnythingControllerGlue(
    content::WebContents* contents,
    ReadAnythingController* controller)
    : content::WebContentsUserData<ReadAnythingControllerGlue>(*contents),
      controller_(controller) {}

// static
bool ReadAnythingController::freeze_distillation_for_testing_ = false;

// static
void ReadAnythingController::
    SetFreezeDistillationOnCreationForTesting(  // IN-TEST
        bool locked) {
  freeze_distillation_for_testing_ = locked;
}

ReadAnythingController* ReadAnythingController::From(tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

ReadAnythingController::ReadAnythingController(
    tabs::TabInterface* tab,
    SidePanelRegistry* side_panel_registry)
    : tabs::ContentsObservingTabFeature(*tab),
      tab_(tab),
      side_panel_registry_(side_panel_registry),
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this),
      read_anything_side_panel_controller_(
          std::make_unique<ReadAnythingSidePanelController>(
              tab,
              side_panel_registry)),
      distillation_state_locked_for_testing_(freeze_distillation_for_testing_) {
  // This controller should only be instantiated if
  // IsImmersiveReadAnythingEnabled is enabled
  CHECK(features::IsImmersiveReadAnythingEnabled());

  // Point the FindBar to IRM's WebContents, if it's open. We already call
  // MaybeUpdateFindBarController when IRM opens and closes, but if IRM is open
  // on a split view, it can stay open even if the tab is not active, so we need
  // this to update the FindBar when the IRM tab is reactivated.
  tab_did_activate_subscription_ = tab_->RegisterDidActivate(
      base::IgnoreArgs<tabs::TabInterface*>(base::BindRepeating(
          &ReadAnythingController::MaybeUpdateFindBarController,
          base::Unretained(this))));

  if (features::IsReadAnythingOmniboxChipEnabled() &&
      base::FeatureList::IsEnabled(features::kPageActionsMigration)) {
    omnibox_controller_ = std::make_unique<ReadAnythingOmniboxController>(tab_);
  }
}

ReadAnythingController::~ReadAnythingController() {
  observers_.Notify(&Observer::OnDestroyed);

  if (GetPresentationState() == PresentationState::kInImmersiveOverlay) {
    CloseImmersiveUI(ReadAnythingCloseReason::kControllerDestroyed);
  }

  // Notify the renderer that we don't need the main webpage treated as
  // visible anymore for IRM. Although we already do this in OnVisibilityChanged
  // when Reading Mode's visibility changes to hidden or occluded, that callback
  // doesn't seem to be reliably called when a tab is closed, so we need to do
  // this here too.
  ReleaseMainContentsCapture();

  if (ra_web_ui_observer_ && ra_web_ui_observer_->web_contents()) {
    ra_web_ui_observer_->web_contents()->RemoveUserData(
        ReadAnythingControllerGlue::UserDataKey());
  }

  // If the Side Panel was showing, it might still hold the WebContents. Ensure
  // the glue is removed from there too.
  read_anything_side_panel_controller_->RemoveReadAnythingControllerGlue();
}

void ReadAnythingController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ReadAnythingController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ReadAnythingController::AddImmersiveActivationObserver(
    ReadAnythingImmersiveActivationObserver* observer) {
  // There should only be one observer at a time. There should never be two
  // components responsible for showing and hiding the same IRM UI.
  CHECK(immersive_activation_observers_.empty());

  immersive_activation_observers_.AddObserver(observer);

  // Now that a tab potentially reattached and was potentially previously
  // showing IRM, we should check if we should show IRM again.
  if (should_show_immersive_on_tab_reactivate_) {
    ShowImmersiveUI(ReadAnythingOpenTrigger::kTabSwitch);
    // Reset value now that the tab is active
    should_show_immersive_on_tab_reactivate_ = false;
  }
}

void ReadAnythingController::RemoveImmersiveActivationObserver(
    ReadAnythingImmersiveActivationObserver* observer) {
  // If the observer detaches, we need to close IRM if showing
  if (GetPresentationState() == PresentationState::kInImmersiveOverlay) {
    CloseImmersiveUI(ReadAnythingCloseReason::kTabSwitched);
  }

  immersive_activation_observers_.RemoveObserver(observer);
}

void ReadAnythingController::OnEntryShown(
    std::optional<ReadAnythingOpenTrigger> trigger) {
  observers_.Notify(&Observer::Activate, /*active=*/true, trigger,
                    /*completed_session_duration=*/std::nullopt);
  active_service_ =
      ReadAnythingService::Get(tab_->GetBrowserWindowInterface()->GetProfile());
  // At the moment, services are created for normal, guest, and incognito
  // profiles but not unusual profile types. On the other hand,
  // ReadAnythingController is created for all tabs. Thus we need a
  // nullptr check.
  if (active_service_) {
    active_service_->OnReadAnythingShown();
  }

  if (is_presentation_transitioning_) {
    is_presentation_transitioning_ = false;
  } else {
    entry_shown_timestamp_ = base::TimeTicks::Now();
  }

  if (auto* user_ed = BrowserUserEducationInterface::From(
          tab_->GetBrowserWindowInterface())) {
    if (GetPresentationState() == PresentationState::kInImmersiveOverlay ||
        GetPresentationState() == PresentationState::kInSidePanel) {
      user_ed->MaybeShowFeaturePromo(
          feature_engagement::kIPHReadingModeKeyboardShortcutFeature);
    }
  }

  MaybeUpdateFindBarController();
}

void ReadAnythingController::OnEntryHidden() {
  std::optional<base::TimeDelta> completed_session_duration =
      RecordEntryHiddenMetrics();

  observers_.Notify(&Observer::Activate, /*active=*/false,
                    /*trigger=*/std::optional<ReadAnythingOpenTrigger>(),
                    completed_session_duration);

  if (active_service_) {
    active_service_->OnReadAnythingHidden();
    active_service_ = nullptr;
  }
}

std::optional<base::TimeDelta>
ReadAnythingController::RecordEntryHiddenMetrics() {
  // If we are transitioning between UI modes (e.g., Side Panel to Immersive),
  // don't record OnEntryHidden metrics.
  if (is_presentation_transitioning_) {
    return std::nullopt;
  }

  // Record whether the UI was closed before it was successfully shown.
  const bool hidden_before_shown = entry_shown_timestamp_.is_null();
  base::UmaHistogramBoolean("Accessibility.ReadAnything.HiddenBeforeShown",
                            hidden_before_shown);

  if (hidden_before_shown) {
    return std::nullopt;
  }

  // Calculate the duration that Reading Mode was visible in this tab session.
  base::TimeDelta completed_session_duration =
      base::TimeTicks::Now() - entry_shown_timestamp_;

  // Only record if RM was successfully shown.
  base::UmaHistogramCustomTimes(
      "Accessibility.ReadAnything.ShownDurationMax1Day",
      completed_session_duration, /*min=*/base::Seconds(1),
      /*max=*/base::Hours(24), /*buckets=*/100);

  // Reset the timestamp to null to avoid polluting data if the next RM show
  // request is closed before it's fully shown.
  entry_shown_timestamp_ = base::TimeTicks();

  return completed_session_duration;
}

// Returns the SidePanelUI for the active tab if the tab is active and has a
// browser window interface. Returns nullptr otherwise.
SidePanelUI* ReadAnythingController::GetSidePanelUI() {
  if (!tab_ || !tab_->IsActivated() || !tab_->GetBrowserWindowInterface()) {
    return nullptr;
  }

  return tab_->GetBrowserWindowInterface()->GetFeatures().side_panel_ui();
}

// Lazily creates and returns the WebUIContentsWrapper for Reading Mode.
std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
ReadAnythingController::GetOrCreateWebUIWrapper(
    PresentationState web_ui_new_presentation_state) {
  SetPresentationState(web_ui_new_presentation_state);
  if (should_recreate_web_ui_ || !web_ui_wrapper_) {
    should_recreate_web_ui_ = false;
    has_shown_ui_ = false;
    Profile* profile = tab_->GetBrowserWindowInterface()->GetProfile();
    web_ui_wrapper_ =
        std::make_unique<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>(
            GURL(chrome::kChromeUIUntrustedReadAnythingSidePanelURL), profile,
            IDS_READING_MODE_TITLE,
            /*esc_closes_ui=*/false);

    ra_web_ui_observer_ = std::make_unique<WebContentsObserverInstance>(
        /*web_contents=*/web_ui_wrapper_->web_contents(),
        /*primary_page_changed_callback=*/base::DoNothing(),
        /*renderer_crashed_callback=*/
        base::BindRepeating(&ReadAnythingController::OnRendererCrashed,
                            base::Unretained(this)),
        /*visibility_changed_callback=*/
        base::BindRepeating(
            &ReadAnythingController::OnReadAnythingVisibilityChanged,
            base::Unretained(this)));

    ReadAnythingControllerGlue::CreateForWebContents(
        web_ui_wrapper_->web_contents(), this);
    find_in_page::FindTabHelper::CreateForWebContents(
        web_ui_wrapper_->web_contents());
  }
  return std::move(web_ui_wrapper_);
}

void ReadAnythingController::RecreateWebUIWrapper() {
  should_recreate_web_ui_ = true;
}

void ReadAnythingController::OnRendererCrashed() {
  // If we determine that the renderer crashed, we need to recreate the WebUI
  // wrapper and ra_web_ui_observer_ the next time it's accessed. Closing the
  // WebUI ensures everything is shut down properly so that reopening will
  // then recreate without issues. This is also how WebUIContentsWrapper handles
  // crashes (see WebUIContentsWrapper::PrimaryMainFrameRenderProcessGone).
  RecreateWebUIWrapper();
  if (GetPresentationState() == PresentationState::kInImmersiveOverlay) {
    CloseImmersiveUI(ReadAnythingCloseReason::kRendererCrashed);
  } else if (GetPresentationState() == PresentationState::kInSidePanel) {
    CloseSidePanelUI(ReadAnythingCloseReason::kRendererCrashed);
  }
}

void ReadAnythingController::SetWebUIWrapperForTest(
    std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
        web_ui_wrapper) {
  web_ui_wrapper_ = std::move(web_ui_wrapper);
}

void ReadAnythingController::TransferWebUiOwnership(
    std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
        web_ui_wrapper,
    PresentationState from_presentation) {
  // Ignore the returned wrapper if it's coming from a UI that is no longer
  // the active presentation.
  if (GetPresentationState() != from_presentation) {
    return;
  }
  CHECK(web_ui_wrapper);
  CHECK(!web_ui_wrapper_);
  web_ui_wrapper_ = std::move(web_ui_wrapper);
  SetPresentationState(PresentationState::kInactive);

  // If the WebUI was never shown, it likely means it was still loading when
  // we closed it. If we reuse this wrapper, we might miss the "ShowUI" signal
  // if it fires while detached. Force recreation to ensure a fresh signal
  // next time.
  if (!has_shown_ui_) {
    RecreateWebUIWrapper();
  }

  // Ensure the find-in-page target is updated to reflect the new presentation
  // state.
  MaybeUpdateFindBarController();
}

void ReadAnythingController::MaybeUpdateFindBarController() {
  if (!tab_ || !tab_->IsActivated() || !tab_->GetBrowserWindowInterface()) {
    return;
  }

  content::WebContents* target_contents;
  if (GetPresentationState() == PresentationState::kInImmersiveOverlay &&
      ra_web_ui_observer_ && ra_web_ui_observer_->web_contents()) {
    target_contents = ra_web_ui_observer_->web_contents();
  } else {
    // We're not in IRM so track main WebContents
    target_contents = tab_->GetContents();
  }

  // If the target is just the main web contents, we don't need to force the
  // FindBarController's creation if it doesn't already exist.
  auto& window_features = tab_->GetBrowserWindowInterface()->GetFeatures();
  if (target_contents == tab_->GetContents() &&
      !window_features.HasFindBarController()) {
    return;
  }

  auto* find_bar_controller = window_features.GetFindBarController();
  if (!find_bar_controller) {
    return;
  }

  // Direct the FindBarController to track the new target WebContents
  if (find_bar_controller->web_contents() != target_contents) {
    find_bar_controller->ChangeWebContents(target_contents);
  }
}

void ReadAnythingController::ShowImmersiveUI(ReadAnythingOpenTrigger trigger) {
  // Show Reading Mode in side panel mode if the distillation is empty and
  // Reading Mode was inactive.
  if (distillation_state_ == DistillationState::kDistillationEmpty &&
      GetPresentationState() == PresentationState::kInactive) {
    base::UmaHistogramEnumeration(
        "Accessibility.ReadAnything.SidePanelTriggeredByEmptyState", trigger);

    SidePanelOpenTrigger side_panel_open_trigger =
        read_anything::ReadAnythingToSidePanelOpenTrigger(trigger);

    ShowSidePanelUI(side_panel_open_trigger);
    return;
  }

  if (GetPresentationState() == PresentationState::kInImmersiveOverlay) {
    return;
  }

  if (GetPresentationState() == PresentationState::kInSidePanel) {
    is_presentation_transitioning_ = true;
    CloseSidePanelUI(ReadAnythingCloseReason::kToggledPresentation);
    // Ensure we got the web_ui_wrapper_ back from the Side Panel if one ever
    // existed.
    CHECK(!has_shown_ui_ || web_ui_wrapper_);
  }

  immersive_activation_observers_.Notify(
      &ReadAnythingImmersiveActivationObserver::OnShowImmersive, trigger);

  // Ensure the observer took the web_ui_wrapper_
  CHECK(!web_ui_wrapper_);
}

void ReadAnythingController::ShowSidePanelUI(SidePanelOpenTrigger trigger) {
  if (GetPresentationState() == PresentationState::kInImmersiveOverlay) {
    is_presentation_transitioning_ = true;
    CloseImmersiveUI(ReadAnythingCloseReason::kToggledPresentation);
    // Ensure we got the web_ui_wrapper_ back from the immersive overlay if one
    // ever existed.
    CHECK(!has_shown_ui_ || web_ui_wrapper_);
  }

  if (SidePanelUI* side_panel_ui = GetSidePanelUI()) {
    side_panel_ui->Show(SidePanelEntryId::kReadAnything, trigger);
  }
}

void ReadAnythingController::CloseImmersiveUI(ReadAnythingCloseReason reason) {
  if (GetPresentationState() != PresentationState::kInImmersiveOverlay) {
    return;
  }

  observers_.Notify(&ReadAnythingLifecycleObserver::OnWillClose, reason);
  immersive_activation_observers_.Notify(
      &ReadAnythingImmersiveActivationObserver::OnCloseImmersive);

  // If a tab switch is the reason we're closing immersive mode, we want to
  // set should_show_immersive_on_tab_reactivate_ so we know to activate
  // immersive mode again if the tab becomes active.
  if (reason == ReadAnythingCloseReason::kTabSwitched) {
    should_show_immersive_on_tab_reactivate_ = true;
  }

  // Ensure the observer returned the web_ui_wrapper_
  CHECK(web_ui_wrapper_);
}

void ReadAnythingController::CloseSidePanelUI(ReadAnythingCloseReason reason) {
  if (GetPresentationState() != PresentationState::kInSidePanel) {
    return;
  }

  if (SidePanelUI* side_panel_ui = GetSidePanelUI()) {
    SidePanelEntryHideReason hide_reason =
        (reason == ReadAnythingCloseReason::kTabSwitched)
            ? SidePanelEntryHideReason::kBackgrounded
            : SidePanelEntryHideReason::kSidePanelClosed;
    side_panel_ui->Close(hide_reason,
                         /*suppress_animations=*/true);
  }
}

void ReadAnythingController::ShowInPreferredUI(
    ReadAnythingOpenTrigger trigger) {
  if (!tab_ || !tab_->GetBrowserWindowInterface()) {
    // This is not likely to happen, but if it ever does, default to showing
    // the UI via ShowImmersiveUI and using any of its error handling.
    ShowImmersiveUI(trigger);
    return;
  }

  PrefService* prefs =
      tab_->GetBrowserWindowInterface()->GetProfile()->GetPrefs();
  if (!prefs) {
    // This is not likely to happen, but if it ever does, default to showing
    // the UI via ShowImmersiveUI.
    ShowImmersiveUI(trigger);
    return;
  }

  int last_presentation_state = prefs->GetInteger(
      prefs::kAccessibilityReadAnythingLastOpenedPresentationState);
  if (last_presentation_state ==
      static_cast<int>(
          read_anything::mojom::ReadAnythingPresentationState::kInSidePanel)) {
    ShowSidePanelUI(read_anything::ReadAnythingToSidePanelOpenTrigger(trigger));
  } else {
    ShowImmersiveUI(trigger);
  }
}

void ReadAnythingController::ToggleUI(ReadAnythingOpenTrigger trigger) {
  if (trigger == ReadAnythingOpenTrigger::kKeyboardShortcut) {
    if (auto* user_ed = BrowserUserEducationInterface::From(
            tab_->GetBrowserWindowInterface())) {
      user_ed->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHReadingModeKeyboardShortcutFeature,
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
    }
  }

  PresentationState state = GetPresentationState();
  if (state == PresentationState::kInImmersiveOverlay) {
    CloseImmersiveUI(ReadAnythingCloseReason::kClosedByUser);
    return;
  }

  if (state == PresentationState::kInSidePanel) {
    CloseSidePanelUI(ReadAnythingCloseReason::kClosedByUser);
    return;
  }

  ShowInPreferredUI(trigger);
}

void ReadAnythingController::TogglePresentation(bool is_user_initiated) {
  if (GetPresentationState() == PresentationState::kInImmersiveOverlay) {
    if (is_user_initiated) {
      SavePresentationPreference(PresentationState::kInSidePanel);
    }
    ShowSidePanelUI(
        SidePanelOpenTrigger::kReadAnythingTogglePresentationButton);
  } else if (GetPresentationState() == PresentationState::kInSidePanel) {
    if (is_user_initiated) {
      SavePresentationPreference(PresentationState::kInImmersiveOverlay);
    }
    ShowImmersiveUI(
        ReadAnythingOpenTrigger::kReadAnythingTogglePresentationButton);
  }
}

void ReadAnythingController::SavePresentationPreference(
    PresentationState state) {
  if (state == PresentationState::kInSidePanel ||
      state == PresentationState::kInImmersiveOverlay) {
    tab_->GetBrowserWindowInterface()->GetProfile()->GetPrefs()->SetInteger(
        prefs::kAccessibilityReadAnythingLastOpenedPresentationState,
        static_cast<int>(state));
  }
}

// TODO(crbug.com/458335664): Add logic to check if IRM SidePanel is showing
ReadAnythingController::PresentationState
ReadAnythingController::GetPresentationState() const {
  return presentation_state_;
}

void ReadAnythingController::SetPresentationState(PresentationState new_state) {
  if (presentation_state_ == new_state) {
    return;
  }
  presentation_state_ = new_state;
  observers_.Notify(&Observer::OnReadingModePresenterChanged);
}

void ReadAnythingController::PrimaryPageChanged(content::Page& page) {
  if (GetPresentationState() == PresentationState::kInImmersiveOverlay) {
    CloseImmersiveUI(ReadAnythingCloseReason::kPageChanged);
  }
}

void ReadAnythingController::OnSoftNavigation() {
  // Soft navigations are Single Page App (SPA) navigations (URL change + DOM
  // update). They are not emitted via PrimaryPageChanged. We handle them
  // here to differentiate them from simple fragment/anchor navigations, which
  // should *not* close Reading Mode. For true soft navigations, we close IRM if
  // it's open
  if (GetPresentationState() == PresentationState::kInImmersiveOverlay) {
    CloseImmersiveUI(ReadAnythingCloseReason::kPageChangedSoftNavigation);
  }
  // Currently, Readability doesn't redistill on soft navigations,
  // so we recreate the WebUI on the next time it's opened to force a redistill.
  RecreateWebUIWrapper();
}

void ReadAnythingController::OnReadAnythingVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE) {
    has_shown_ui_ = true;
    // When IRM is being shown either for the first time or after being visible
    // again after being occluded, we tell the renderer that the main webpage
    // needs to be treated as visible even though it's occluded, so it can
    // generate accessibility events we need for RM to function.
    if (GetPresentationState() == PresentationState::kInImmersiveOverlay) {
      CaptureMainContentsAsVisible();
    }
  } else {
    // We don't need the main web contents treated as visible anymore because
    // Reading Mode is hidden or occluded.
    ReleaseMainContentsCapture();
  }
}

void ReadAnythingController::CaptureMainContentsAsVisible() {
  // Don't increment the capturer count of the main contents if we already are
  // capturing it.
  if (main_contents_capturer_handle_) {
    return;
  }

  // To let the renderer know that the content is still needed, we increment the
  // capturer count and set `stay_hidden` to false, which forces the renderer to
  // treat the content as visible.
  main_contents_capturer_handle_ = tab_->GetContents()->IncrementCapturerCount(
      gfx::Size(), /*stay_hidden=*/false, /*stay_awake=*/false,
      /*is_activity=*/true);
}

void ReadAnythingController::ReleaseMainContentsCapture() {
  main_contents_capturer_handle_.RunAndReset();
}



void ReadAnythingController::OnDistillationStateChanged(
    DistillationState new_state) {
  if (distillation_state_locked_for_testing_) {
    return;
  }

  if (new_state == DistillationState::kDistillationEmpty &&
      GetPresentationState() == PresentationState::kInImmersiveOverlay) {
    base::UmaHistogramEnumeration(
        "Accessibility.ReadAnything.SidePanelTriggeredByEmptyState",
        ReadAnythingOpenTrigger::kReadAnythingTogglePresentationButton);

    TogglePresentation(/*is_user_initiated=*/false);
  }
  distillation_state_ = new_state;
}

void ReadAnythingController::UnlockDistillationStateForTesting() {
  distillation_state_locked_for_testing_ = false;
}

void ReadAnythingController::SetDwellTimeForTesting(base::TimeTicks test_time) {
  omnibox_controller_->SetDwellTimeForTesting(test_time);
}
