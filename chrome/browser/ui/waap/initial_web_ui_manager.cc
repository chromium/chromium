// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/initial_web_ui_manager.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

// Forward declaration to avoid circular dependency with //chrome/browser.
// This function is defined in
// //chrome/browser/page_load_metrics/page_load_metrics_initialize.cc
void InitializePageLoadMetricsForWebContents(
    content::WebContents* web_contents);

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
    scoped_refptr<content::SiteInstance> site_instance =
        content::SiteInstance::CreateForURL(
            profile, GURL(chrome::kChromeUIWebUIToolbarURL));
    toolbar_web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile, site_instance));

    ConfigureToolbarWebContents(toolbar_web_contents_.get(), browser);

    toolbar_web_contents_->GetController().LoadURL(
        GURL(chrome::kChromeUIWebUIToolbarURL), content::Referrer(),
        ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
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
  // PLM has to be initialized before loading the URL.
  InitializePageLoadMetricsForWebContents(web_contents);
  // Needed for UKM PageLoad metrics.
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents);

  web_contents->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  web_contents->SetIgnoreZoomGestures(true);

  // Ensure the browser window interface is associated with the WebContents
  // before the WebUI acts on it.
  webui::SetBrowserWindowInterface(web_contents, browser);
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

void InitialWebUIManager::OnWebUIToolbarLoaded() {
  is_initial_web_ui_pending_ = false;
  is_show_pending_ = false;
  web_ui_ready_callbacks_.Notify();
}
