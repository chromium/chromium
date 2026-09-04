// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/initial_web_ui_manager.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_webui_profile_service.h"
#include "chrome/browser/ui/waap/initial_webui_profile_service_factory.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"
#include "chrome/browser/ui/waap/waap_utils.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"

DEFINE_USER_DATA(InitialWebUIManager);

InitialWebUIManager::InitialWebUIManager(BrowserWindowInterface* browser)
    : is_initial_web_ui_pending_(
          features::IsWebUIToolbarEnabled() ||
          base::FeatureList::IsEnabled(
              features::kWebUIToolbarProcessOverheadExperiment)),
      scoped_data_holder_(browser->GetUnownedUserDataHost(), *this),
      metrics_manager_(InitialWebUIWindowMetricsManager::From(browser)) {
  if ((features::IsWebUIToolbarEnabled() &&
       features::kWebUIReloadButtonPrewarmWebUI.Get()) ||
      base::FeatureList::IsEnabled(
          features::kWebUIToolbarProcessOverheadExperiment)) {
    Profile* profile = browser->GetProfile();

    if (features::kWebUIReloadButtonProfilePrewarming.Get()) {
      // If `WebUIReloadButtonProfilePrewarming` is enabled, the WebContents is
      // created and pre-navigated in the profile service, so we just take it
      // from there.
      if (auto* profile_service =
              InitialWebUIProfileServiceFactory::GetForProfile(profile)) {
        toolbar_web_contents_ = profile_service->TakeToolbarContents();
      }
    }

    if (toolbar_web_contents_) {
      // The WebContents is already pre-created, bind the BrowserWindowInterface
      // now.
      webui::SetBrowserWindowInterface(toolbar_web_contents_.get(), browser);
      return;
    }

    const bool pre_navigate =
        features::kWebUIReloadButtonPrewarmWebUIPreNavigate.Get() ||
        base::FeatureList::IsEnabled(
            features::kWebUIToolbarProcessOverheadExperiment);
    toolbar_web_contents_ =
        waap::PrewarmHelper::PrewarmWebUIContents(profile, pre_navigate);
    webui::SetBrowserWindowInterface(toolbar_web_contents_.get(), browser);
  }
}

InitialWebUIManager::~InitialWebUIManager() = default;

std::unique_ptr<content::WebContents>
InitialWebUIManager::TakeToolbarContents() {
  return std::move(toolbar_web_contents_);
}

// static
InitialWebUIManager* InitialWebUIManager::From(
    BrowserWindowInterface* browser_window_interface) {
  return Get(browser_window_interface->GetUnownedUserDataHost());
}

// static
void InitialWebUIManager::ConfigureToolbarWebContents(
    content::WebContents* web_contents,
    BrowserWindowInterface* browser) {
  waap::PrewarmHelper::ConfigureWebUIContents(web_contents,
                                              browser->GetProfile());
  // Ensure the browser window interface is associated with the WebContents
  // before the WebUI acts on it.
  webui::SetBrowserWindowInterface(web_contents, browser);
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      web_contents, SK_ColorTRANSPARENT);
}

bool InitialWebUIManager::RequestDeferShow(base::OnceClosure unsafe_callback) {
  if (metrics_manager_) {
    metrics_manager_->OnBrowserWindowShowRequested(base::TimeTicks::Now());
  }
  if (!base::FeatureList::IsEnabled(features::kWebUIReloadButton) ||
      !features::kWebUIReloadButtonDeferBrowserViewShow.Get()) {
    // Do not defer if the experiment is disabled or the param is false.
    return false;
  }
  if (is_initial_web_ui_pending_) {
    is_show_pending_ = true;
    if (unsafe_callback) {
      web_ui_ready_callbacks_.AddUnsafe(std::move(unsafe_callback));
    }
    return true;
  }
  return false;
}

bool InitialWebUIManager::IsShowPending() const {
  return is_show_pending_;
}

bool InitialWebUIManager::IsInitialWebUIPending() const {
  return is_initial_web_ui_pending_;
}

void InitialWebUIManager::OnWebUIToolbarLoaded() {
  is_initial_web_ui_pending_ = false;
  is_show_pending_ = false;
  web_ui_ready_callbacks_.Notify();
}
