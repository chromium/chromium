// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/accessibility_features.h"

///////////////////////////////////////////////////////////////////////////////
// WebContentsObserverInstance

WebContentsObserverInstance::WebContentsObserverInstance(
    content::WebContents* web_contents,
    base::RepeatingClosure primary_page_changed_callback,
    base::RepeatingCallback<void(content::Visibility)>
        visibility_changed_callback)
    : content::WebContentsObserver(web_contents),
      primary_page_changed_callback_(primary_page_changed_callback),
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

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingController

DEFINE_USER_DATA(ReadAnythingController);

WEB_CONTENTS_USER_DATA_KEY_IMPL(ReadAnythingControllerGlue);

ReadAnythingControllerGlue::ReadAnythingControllerGlue(
    content::WebContents* contents,
    ReadAnythingController* controller)
    : content::WebContentsUserData<ReadAnythingControllerGlue>(*contents),
      controller_(controller) {}

ReadAnythingController* ReadAnythingController::From(tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

ReadAnythingController::ReadAnythingController(
    tabs::TabInterface* tab,
    SidePanelRegistry* side_panel_registry)
    : tab_(tab),
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this),
      read_anything_side_panel_controller_(
          std::make_unique<ReadAnythingSidePanelController>(
              tab,
              side_panel_registry)) {
  // This controller should only be instantiated if
  // IsImmersiveReadAnythingEnabled is enabled
  CHECK(features::IsImmersiveReadAnythingEnabled());

  if (tab_->GetBrowserWindowInterface() &&
      tab_->GetBrowserWindowInterface()->GetTabStripModel()) {
    tab_->GetBrowserWindowInterface()->GetTabStripModel()->AddObserver(this);
  }

  tab_subscriptions_.push_back(
      tab_->RegisterWillDetach(base::BindRepeating(
          &ReadAnythingController::TabWillDetach, weak_factory_.GetWeakPtr())));

  main_page_observer_ = std::make_unique<WebContentsObserverInstance>(
      /*web_contents=*/tab_->GetContents(),
      /*primary_page_changed_callback=*/
      base::BindRepeating(&ReadAnythingController::OnMainPagePrimaryPageChanged,
                          base::Unretained(this)),
      /*visibility_changed_callback=*/base::DoNothing());
}

ReadAnythingController::~ReadAnythingController() {
  observers_.Notify(&Observer::OnDestroyed);

  if (tab_->GetBrowserWindowInterface() &&
      tab_->GetBrowserWindowInterface()->GetTabStripModel()) {
    tab_->GetBrowserWindowInterface()->GetTabStripModel()->RemoveObserver(this);
  }

  // This method is transiently used to reset features that do not handle tab
  // discarding themselves.
  read_anything_side_panel_controller_->ResetForTabDiscard();

  if (ra_web_ui_observer_ && ra_web_ui_observer_->web_contents()) {
    ra_web_ui_observer_->web_contents()->RemoveUserData(
        ReadAnythingControllerGlue::UserDataKey());
  }
}

void ReadAnythingController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ReadAnythingController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ReadAnythingController::OnEntryShown(
    std::optional<ReadAnythingOpenTrigger> trigger) {
  observers_.Notify(&Observer::Activate, true, trigger);
}

void ReadAnythingController::OnEntryHidden() {
  observers_.Notify(&Observer::Activate, false,
                    std::optional<ReadAnythingOpenTrigger>());
}

void ReadAnythingController::TabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  observers_.Notify(&Observer::OnTabWillDetach);
}

void ReadAnythingController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection_change) {
  // TODO(crbug.com/462754391): Add logic to prevent marking a tab as inactive
  // when the user is in split view and the tab is still visible.
  if (selection_change.active_tab_changed()) {
    // Handle when this controller's tab becomes active, or when this
    // controller's tab is the previous active tab.
    if (tab_->GetContents() == selection_change.new_contents) {
      // TODO(crbug.com/463730426): ReadAnythingController should decide whether
      // to show Reading Mode when tab becomes active.
      is_active_tab_ = true;
    } else if (tab_->GetContents() == selection_change.old_contents &&
               change.type() != TabStripModelChange::kRemoved) {
      // TODO(crbug.com/463730426): ReadAnythingController should close
      // immersive reading mode when a tab becomes inactive.
      is_active_tab_ = false;
    }
  }
}

bool ReadAnythingController::isActiveTab() {
  return is_active_tab_;
}

int ReadAnythingController::GetNavCounterForTesting() const {
  return nav_counter_;
}

// Returns the SidePanelUI for the active tab if the tab is active and has a
// browser window interface. Returns nullptr otherwise.
SidePanelUI* ReadAnythingController::GetSidePanelUI() {
  CHECK(tab_);
  CHECK(tab_->IsActivated());
  CHECK(tab_->GetBrowserWindowInterface());

  return tab_->GetBrowserWindowInterface()->GetFeatures().side_panel_ui();
}

// Lazily creates and returns the WebUIContentsWrapper for Reading Mode.
std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
ReadAnythingController::GetOrCreateWebUIWrapper(
    PresentationState web_ui_new_presentation_state) {
  presentation_state_ = web_ui_new_presentation_state;
  if (!web_ui_wrapper_) {
    Profile* profile = tab_->GetBrowserWindowInterface()->GetProfile();
    web_ui_wrapper_ =
        std::make_unique<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>(
            GURL(chrome::kChromeUIUntrustedReadAnythingSidePanelURL), profile,
            IDS_READING_MODE_TITLE,
            /*esc_closes_ui=*/false);

    ra_web_ui_observer_ = std::make_unique<WebContentsObserverInstance>(
        /*web_contents=*/web_ui_wrapper_->web_contents(), base::DoNothing(),
        /*primary_page_changed_callback=*/
        base::BindRepeating(
            &ReadAnythingController::OnReadAnythingVisibilityChanged,
            /*visibility_changed_callback=*/base::Unretained(this)));

    ReadAnythingControllerGlue::CreateForWebContents(
        web_ui_wrapper_->web_contents(), this);
  }
  return std::move(web_ui_wrapper_);
}

void ReadAnythingController::SetWebUIWrapperForTest(
    std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
        web_ui_wrapper) {
  web_ui_wrapper_ = std::move(web_ui_wrapper);
}

void ReadAnythingController::TransferWebUiOwnership(
    std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
        web_ui_wrapper) {
  CHECK(!web_ui_wrapper_);
  web_ui_wrapper_ = std::move(web_ui_wrapper);
  presentation_state_ = PresentationState::kInactive;
}

// TODO(crbug.com/447418049): Open immersive reading mode via this
// entrypoint. Currently just open side panel reading mode via
// ReadAnythingController when is_immersive_read_anything_enabled_ flag is
// enabled.
void ReadAnythingController::ShowUI(SidePanelOpenTrigger trigger) {
  if (SidePanelUI* side_panel_ui = GetSidePanelUI()) {
    side_panel_ui->Show(SidePanelEntryId::kReadAnything, trigger);
  }
}

// TODO(crbug.com/447418049): Toggle immersive reading mode via this
// entrypoint. Currently just toggle side panel reading mode via
// ReadAnythingController when is_immersive_read_anything_enabled_ flag is
// enabled.
void ReadAnythingController::ToggleReadAnythingSidePanel(
    SidePanelOpenTrigger trigger) {
  if (SidePanelUI* side_panel_ui = GetSidePanelUI()) {
    side_panel_ui->Toggle(SidePanelEntryKey(SidePanelEntryId::kReadAnything),
                          trigger);
  }
}

// TODO(crbug.com/458335664): Add logic to check if IRM SidePanel is showing
ReadAnythingController::PresentationState
ReadAnythingController::GetPresentationState() const {
  return presentation_state_;
}

void ReadAnythingController::OnMainPagePrimaryPageChanged() {
  // TODO(crbug.com/460136558): Implement showing/hiding when this function
  // gets called. Update tests to check if RM is visible / hidden instead of
  // nav_counter check.
  nav_counter_++;
}
void ReadAnythingController::OnReadAnythingVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE) {
    has_shown_ui_ = true;
  }
}
