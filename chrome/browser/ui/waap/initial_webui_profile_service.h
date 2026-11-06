// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WAAP_INITIAL_WEBUI_PROFILE_SERVICE_H_
#define CHROME_BROWSER_UI_WAAP_INITIAL_WEBUI_PROFILE_SERVICE_H_

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_prerenderer.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/color/color_provider_key.h"

class Profile;
class ProfileBrowserCollection;

namespace content {
class WebContents;
}  // namespace content

// InitialWebUIProfileService manages prewarmed WebUI toolbars for a profile.
//
// With `WebUIReloadButtonProfilePrewarming` and prerendering disabled, it
// pre-creates and navigates a single prewarmed toolbar during Profile
// initialization for the profile's first window to adopt.
//
// With `TopChromeWebUIPrerendering`, the service builds no toolbar at profile
// creation; the first window builds its own toolbar, and its take call
// schedules the first refill. After each take, a `WebUIContentsPrerenderer`
// keeps a prewarmed toolbar ready for the next window, and the service builds
// it and checks that what it read is still current. The service records each
// take and drops the prewarmed toolbar when the profile's last window closes.
class InitialWebUIProfileService : public KeyedService,
                                   public BrowserCollectionObserver,
                                   public WebUIContentsPrerenderer::Delegate {
 public:
  // What a prewarmed toolbar reads when it navigates and does not update until
  // a window adopts it.
  struct ToolbarInputs {
    ui::ColorProviderKey color_provider_key;
    // The Glic and avatar button eligibility. Neither changes often: avatar
    // eligibility is fixed for a regular profile, and Glic eligibility changes
    // with the user's Glic consent.
    WebUIToolbarUI::ProfileButtonEligibility button_eligibility;
    // Read into the toolbar's initial state.
    bool show_home_button = false;
    // Read into the toolbar's initial state and layout constants stylesheet.
    bool touch_ui = false;

    bool operator==(const ToolbarInputs&) const = default;
  };

  explicit InitialWebUIProfileService(Profile* profile);
  InitialWebUIProfileService(const InitialWebUIProfileService&) = delete;
  InitialWebUIProfileService& operator=(const InitialWebUIProfileService&) =
      delete;
  ~InitialWebUIProfileService() override;

  // Returns whether the profile keeps a prewarmed toolbar for each next
  // window: the WebUI toolbar, `kWebUIReloadButton` with its
  // `WebUIReloadButtonPrewarmWebUI` and `WebUIReloadButtonProfilePrewarming`
  // params, and `kTopChromeWebUIPrerendering` are all enabled.
  static bool IsPrerenderingEnabled();

  // Returns the pre-created WebContents, passing ownership to the caller.
  // Returns nullptr if it has already been taken or prewarming is disabled.
  // With prerendering enabled, also returns nullptr if no prewarmed toolbar
  // exists yet (such as for the profile's first window), or if the pre-created
  // WebContents is out of date or its renderer is gone, and schedules the next
  // prewarmed toolbar.
  std::unique_ptr<content::WebContents> TakeToolbarContents();

  // KeyedService:
  void Shutdown() override;

  // BrowserCollectionObserver:
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

  // TODO(crbug.com/573441656): Retire this per-profile spare after Phase 2,
  // once a spare with no profile serves new windows.
  // WebUIContentsPrerenderer::Delegate:
  std::unique_ptr<content::WebContents> CreateSpare() override;
  bool IsSpareCurrent(content::WebContents& spare) override;

  bool has_toolbar_contents_for_testing() const {
    return toolbar_prerenderer_ ? toolbar_prerenderer_->has_spare()
                                : toolbar_web_contents_ != nullptr;
  }

  content::WebContents* toolbar_web_contents_for_testing() {
    return toolbar_prerenderer_ ? toolbar_prerenderer_->spare_for_testing()
                                : toolbar_web_contents_.get();
  }

  // Null unless prerendering is enabled.
  WebUIContentsPrerenderer* toolbar_prerenderer_for_testing() {
    return toolbar_prerenderer_.get();
  }

  ToolbarInputs GetToolbarInputsForTesting() const {
    return GetToolbarInputs();
  }
  void set_spare_inputs_for_testing(ToolbarInputs inputs) {
    spare_inputs_ = std::move(inputs);
  }
  static WebUIContentsPrerenderer::Params GetPrerenderParamsForTesting() {
    return GetPrerenderParams();
  }

 private:
  void PrewarmWebUI();

  // Returns whether the profile has a normal browser window.
  bool HasNormalBrowser() const;

  ToolbarInputs GetToolbarInputs() const;

  // Returns the refill timing and memory pressure level that the
  // `TopChromeWebUIPrerendering` params set.
  static WebUIContentsPrerenderer::Params GetPrerenderParams();

  const raw_ptr<Profile> profile_;
  // Set with prerendering disabled.
  std::unique_ptr<content::WebContents> toolbar_web_contents_;
  // Set with prerendering enabled.
  std::unique_ptr<WebUIContentsPrerenderer> toolbar_prerenderer_;
  // What the last spare read when it navigated.
  std::optional<ToolbarInputs> spare_inputs_;
  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

#endif  // CHROME_BROWSER_UI_WAAP_INITIAL_WEBUI_PROFILE_SERVICE_H_
