// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/initial_webui_profile_service.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/metrics/histogram_functions.h"
#include "base/supports_user_data.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_collection.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/waap/waap_utils.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_profile.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_prerenderer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/native_theme/native_theme.h"

namespace {

using MemoryPressureLevel =
    features::TopChromeWebUIPrerenderingMemoryPressureLevel;

// Returns the memory limit at or below which `level` applies, or nullopt for
// `kNever`.
std::optional<base::MemoryLimit> ToMemoryLimit(MemoryPressureLevel level) {
  switch (level) {
    case MemoryPressureLevel::kNever:
      return std::nullopt;
    case MemoryPressureLevel::kModerate:
      return base::MemoryLimit::ModeratePressureThreshold();
    case MemoryPressureLevel::kCritical:
      return base::MemoryLimit::CriticalPressureThreshold();
  }
}

// Owns the unbound `InitialWebUIProfileService` on `TopChromeWebUIProfile` and
// destroys it in `OnProfileWillBeDestroyed()`, before
// `TopChromeWebUIProfileImpl` shuts down its storage partitions.
class UnboundServiceHolder : public base::SupportsUserData::Data,
                             public ProfileObserver {
 public:
  static constexpr char kUserDataKey[] =
      "initial_webui_unbound_profile_service";

  explicit UnboundServiceHolder(Profile* top_chrome_profile)
      : service_(
            std::make_unique<InitialWebUIProfileService>(top_chrome_profile)) {
    profile_observation_.Observe(top_chrome_profile);
  }

  ~UnboundServiceHolder() override { DestroyService(); }

  InitialWebUIProfileService* service() { return service_.get(); }

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override { DestroyService(); }

 private:
  void DestroyService() {
    profile_observation_.Reset();
    if (service_) {
      service_->Shutdown();
      service_.reset();
    }
  }

  std::unique_ptr<InitialWebUIProfileService> service_;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
};

}  // namespace

InitialWebUIProfileService::InitialWebUIProfileService(Profile* profile)
    : profile_(profile) {
  if (features::kWebUIReloadButtonProfilePrewarming.Get()) {
    if (IsPrerenderingEnabled()) {
      // With prerendering enabled, no toolbar is built at profile creation; the
      // first window builds its own toolbar and schedules the first refill. The
      // service observes browser windows to drop the spare when the last normal
      // window closes.
      toolbar_prerenderer_ = std::make_unique<WebUIContentsPrerenderer>(
          *this, GetPrerenderParams());
      if (BrowserCollection* browsers = GetBrowserCollection()) {
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

// static
bool InitialWebUIProfileService::IsUnboundPrerenderingEnabled() {
  return IsPrerenderingEnabled() && webui::ShouldUseTopChromeWebUIProfile() &&
         features::kWebUIReloadButtonPrewarmWebUIPreNavigate.Get() &&
         features::kTopChromeWebUIPrerenderingUnbound.Get();
}

// static
InitialWebUIProfileService*
InitialWebUIProfileService::GetForTopChromeWebUIProfile() {
  CHECK(IsUnboundPrerenderingEnabled());
  Profile* profile = webui::GetTopChromeWebUIProfile();
  if (!profile || profile->ShutdownStarted()) {
    return nullptr;
  }
  auto* holder = static_cast<UnboundServiceHolder*>(
      profile->GetUserData(UnboundServiceHolder::kUserDataKey));
  if (!holder) {
    auto new_holder = std::make_unique<UnboundServiceHolder>(profile);
    holder = new_holder.get();
    profile->SetUserData(UnboundServiceHolder::kUserDataKey,
                         std::move(new_holder));
  }
  return holder->service();
}

void InitialWebUIProfileService::Shutdown() {
  browser_collection_observation_.Reset();
  if (toolbar_prerenderer_) {
    toolbar_prerenderer_->Reset();
  }
  toolbar_web_contents_.reset();
}

std::unique_ptr<content::WebContents>
InitialWebUIProfileService::TakeToolbarContents(Profile& window_profile) {
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
  if (outcome.contents && is_unbound()) {
    const bool colors_changed =
        waap::PrewarmHelper::BindUserProfile(*outcome.contents, window_profile);
    WebUIToolbarUI::OnUserProfileBound(*outcome.contents, window_profile,
                                       colors_changed);
  }
  return std::move(outcome.contents);
}

void InitialWebUIProfileService::OnBrowserClosed(
    BrowserWindowInterface* browser) {
  if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL ||
      HasNormalBrowser()) {
    return;
  }
  // The last normal window (for this profile, or across all profiles in unbound
  // mode) is closing. Do not keep a toolbar, and the top-chrome renderer it
  // lives in, alive for a window that may never come. The next window builds
  // its own toolbar and restarts the refill.
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

bool InitialWebUIProfileService::is_unbound() const {
  return profile_->IsTopChromeWebUIProfile();
}

BrowserCollection* InitialWebUIProfileService::GetBrowserCollection() const {
  return is_unbound() ? static_cast<BrowserCollection*>(
                            GlobalBrowserCollection::GetInstance())
                      : ProfileBrowserCollection::GetForProfile(profile_);
}

std::unique_ptr<content::WebContents>
InitialWebUIProfileService::CreateSpare() {
  // No window would adopt a toolbar built while the browser quits or while no
  // normal browser window is open.
  if (browser_shutdown::IsTryingToQuit() || !HasNormalBrowser()) {
    return nullptr;
  }
  TRACE_EVENT("waap", "InitialWebUIProfileService::CreateSpare");
  // A window may take the toolbar long after it navigates, so
  // `IsSpareCurrent()` checks that these have not changed.
  spare_inputs_ = GetToolbarInputs();
  if (is_unbound()) {
    Profile& template_profile = GetTemplateProfile();
    return waap::PrewarmHelper::PrewarmUnboundWebUIContents(
        template_profile, [&](content::WebContents& contents) {
          WebUIToolbarUnboundSeedUserData::CreateForWebContents(
              &contents, template_profile);
        });
  }
  return waap::PrewarmHelper::PrewarmWebUIContents(
      profile_, features::kWebUIReloadButtonPrewarmWebUIPreNavigate.Get());
}

bool InitialWebUIProfileService::IsSpareCurrent(content::WebContents& spare) {
  return spare_inputs_ && *spare_inputs_ == GetToolbarInputs();
}

InitialWebUIProfileService::ToolbarInputs
InitialWebUIProfileService::GetToolbarInputs() const {
  if (is_unbound()) {
    ui::ColorProviderKey baseline_key =
        ui::NativeTheme::GetInstanceForNativeUi()->GetColorProviderKey(nullptr);
    // Creating a new profile on Linux initializes ThemeServiceAuraLinux, which
    // calls NotifyOnNativeThemeUpdated() and bumps `system_theme_version`
    // without changing any baseline theme property. Zero that noisy counter so
    // a newly created profile adopts the spare while keeping every other
    // current and future ColorProviderKey field in the staleness check;
    // BindUserProfile() still compares the full key to decide whether to send
    // `colorsCss`.
    baseline_key.system_theme_version = 0;
    return {
        .color_provider_key = std::move(baseline_key),
        .touch_ui = ui::TouchUiController::Get()->touch_ui(),
    };
  }
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
      // The skip param has no "never" option.
      .skip_memory_limit = *ToMemoryLimit(
          features::kTopChromeWebUIPrerenderingSkipMemoryPressure.Get()),
      .evict_memory_limit = ToMemoryLimit(
          features::kTopChromeWebUIPrerenderingEvictMemoryPressure.Get()),
      .consumer_name = "InitialWebUIProfileService",
  };
}

bool InitialWebUIProfileService::HasNormalBrowser() const {
  BrowserCollection* browsers = GetBrowserCollection();
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

Profile& InitialWebUIProfileService::GetTemplateProfile() const {
  BrowserCollection* browsers = GetBrowserCollection();
  CHECK(browsers);
  Profile* template_profile = nullptr;
  browsers->ForEach(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
          return true;
        }
        Profile* profile = browser->GetProfile();
        CHECK(profile);
        template_profile = profile->GetOriginalProfile();
        return false;
      },
      BrowserCollection::Order::kActivation);
  CHECK(template_profile);
  return *template_profile;
}
