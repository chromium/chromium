// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/initial_webui_profile_service.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/waap/waap_utils.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_prerenderer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/pointer/touch_ui_controller.h"

InitialWebUIProfileService::InitialWebUIProfileService(Profile* profile)
    : profile_(profile) {
  if (features::kWebUIReloadButtonProfilePrewarming.Get()) {
    if (IsPrerenderingEnabled()) {
      // With prerendering enabled, no toolbar is built at profile creation; the
      // profile's first window builds its own toolbar and schedules the first
      // refill. The service observes browser windows to drop the spare when the
      // last window closes.
      toolbar_prerenderer_ = std::make_unique<WebUIContentsPrerenderer>(
          *this, GetPrerenderParams());
      if (ProfileBrowserCollection* browsers =
              ProfileBrowserCollection::GetForProfile(profile_)) {
        browser_collection_observation_.Observe(browsers);
      }
    } else {
      PrewarmWebUI();
    }
  }
}

InitialWebUIProfileService::~InitialWebUIProfileService() = default;

// static
bool InitialWebUIProfileService::IsPrerenderingEnabled() {
  return features::IsWebUIToolbarEnabled() &&
         base::FeatureList::IsEnabled(features::kWebUIReloadButton) &&
         features::kWebUIReloadButtonPrewarmWebUI.Get() &&
         features::kWebUIReloadButtonProfilePrewarming.Get() &&
         base::FeatureList::IsEnabled(features::kTopChromeWebUIPrerendering);
}

void InitialWebUIProfileService::Shutdown() {
  browser_collection_observation_.Reset();
  if (toolbar_prerenderer_) {
    toolbar_prerenderer_->Reset();
  }
  toolbar_web_contents_.reset();
}

std::unique_ptr<content::WebContents>
InitialWebUIProfileService::TakeToolbarContents() {
  TRACE_EVENT("waap", "InitialWebUIProfileService::TakeToolbarContents",
              "has_prewarmed_contents",
              toolbar_prerenderer_ ? toolbar_prerenderer_->has_spare()
                                   : toolbar_web_contents_ != nullptr);
  if (!toolbar_prerenderer_) {
    return std::move(toolbar_web_contents_);
  }

  WebUIContentsPrerenderer::TakeOutcome outcome = toolbar_prerenderer_->Take();
  base::UmaHistogramEnumeration("InitialWebUI.Toolbar.ProfilePrewarmTakeResult",
                                outcome.result);
  toolbar_prerenderer_->ScheduleRefill(outcome.contents.get());
  return std::move(outcome.contents);
}

void InitialWebUIProfileService::OnBrowserClosed(
    BrowserWindowInterface* browser) {
  if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL ||
      HasNormalBrowser()) {
    return;
  }
  // The profile's last window is closing. Do not keep a toolbar, and the
  // top-chrome renderer it lives in, alive for a window that may never come.
  // The next window builds its own toolbar and restarts the refill.
  toolbar_prerenderer_->Reset();
}

void InitialWebUIProfileService::PrewarmWebUI() {
  if (features::IsWebUIToolbarEnabled() &&
      features::kWebUIReloadButtonPrewarmWebUI.Get()) {
    const bool pre_navigate =
        features::kWebUIReloadButtonPrewarmWebUIPreNavigate.Get();
    toolbar_web_contents_ =
        waap::PrewarmHelper::PrewarmWebUIContents(profile_, pre_navigate);
  }
}

std::unique_ptr<content::WebContents>
InitialWebUIProfileService::CreateSpare() {
  // No window would adopt a toolbar built while the browser quits or while
  // the profile has no window.
  if (browser_shutdown::IsTryingToQuit() || !HasNormalBrowser()) {
    return nullptr;
  }
  TRACE_EVENT("waap", "InitialWebUIProfileService::CreateSpare");
  // A window may take the toolbar long after it navigates, so
  // `IsSpareCurrent()` checks that these have not changed.
  spare_inputs_ = GetToolbarInputs();
  return waap::PrewarmHelper::PrewarmWebUIContents(
      profile_, features::kWebUIReloadButtonPrewarmWebUIPreNavigate.Get());
}

bool InitialWebUIProfileService::IsSpareCurrent(content::WebContents& spare) {
  return spare_inputs_ && *spare_inputs_ == GetToolbarInputs();
}

InitialWebUIProfileService::ToolbarInputs
InitialWebUIProfileService::GetToolbarInputs() const {
  return {
      .color_provider_key = waap::PrewarmHelper::GetColorProviderKey(profile_),
      .button_eligibility =
          WebUIToolbarUI::GetProfileButtonEligibility(profile_),
      .show_home_button =
          profile_->GetPrefs()->GetBoolean(prefs::kShowHomeButton),
      .touch_ui = ui::TouchUiController::Get()->touch_ui(),
  };
}

// static
WebUIContentsPrerenderer::Params
InitialWebUIProfileService::GetPrerenderParams() {
  return {
      // Upper bound on how long a refill waits for the taken toolbar to paint.
      // A window that never shows, closes early, or built its own toolbar
      // still gets a prewarmed toolbar ready for the next window after this.
      .paint_deadline =
          features::kTopChromeWebUIPrerenderingPaintDeadline.Get(),
      .delay = features::kTopChromeWebUIPrerenderingDelay.Get(),
      .skip_memory_limit =
          features::kTopChromeWebUIPrerenderingMemoryPressure.Get() ==
                  features::TopChromeWebUIPrerenderingMemoryPressureLevel::
                      kModerate
              ? base::MemoryLimit::ModeratePressureThreshold()
              : base::MemoryLimit::CriticalPressureThreshold(),
      .consumer_name = "InitialWebUIProfileService",
  };
}

bool InitialWebUIProfileService::HasNormalBrowser() const {
  ProfileBrowserCollection* browsers =
      ProfileBrowserCollection::GetForProfile(profile_);
  if (!browsers) {
    return false;
  }
  bool found = false;
  browsers->ForEach([&](BrowserWindowInterface* browser) {
    found = browser->GetType() == BrowserWindowInterface::TYPE_NORMAL;
    return !found;
  });
  return found;
}
