// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_instant_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"

// BrowserInstantController ---------------------------------------------------

BrowserInstantController::BrowserInstantController(Browser* browser)
    : browser_(browser), instant_(profile(), browser_->tab_strip_model()) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  // TemplateURLService can be null in tests.
  if (template_url_service) {
    search_engine_base_url_tracker_ =
        std::make_unique<SearchEngineBaseURLTracker>(
            template_url_service, std::make_unique<UIThreadSearchTermsData>(),
            base::BindRepeating(
                &BrowserInstantController::OnSearchEngineBaseURLChanged,
                base::Unretained(this)));
  }
}

BrowserInstantController::~BrowserInstantController() = default;

void BrowserInstantController::OnSearchEngineBaseURLChanged(
    SearchEngineBaseURLTracker::ChangeReason change_reason) {
  TabStripModel* tab_model = browser_->tab_strip_model();
  int count = tab_model->count();
  for (int index = 0; index < count; ++index) {
    content::WebContents* contents = tab_model->GetWebContentsAt(index);
    if (!contents)
      continue;

    GURL site_url =
        contents->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL();
    bool is_ntp = site_url == GURL(chrome::kChromeUINewTabPageURL) ||
                  site_url == GURL(chrome::kChromeUINewTabPageThirdPartyURL);

    if (!is_ntp) {
      InstantService* instant_service =
          InstantServiceFactory::GetForProfile(profile());
      if (instant_service) {
        content::RenderProcessHost* rph =
            contents->GetPrimaryMainFrame()->GetProcess();
        is_ntp = instant_service->IsInstantProcess(rph->GetID());
      }
    }

    if (!is_ntp)
      continue;

    // When default search engine is changed navigate to chrome://newtab which
    // will redirect to the new tab page associated with the search engine.
    GURL url(chrome::kChromeUINewTabURL);
    content::NavigationController::LoadURLParams params(url);
    params.should_replace_current_entry = true;
    params.referrer = content::Referrer();
    params.transition_type = ui::PAGE_TRANSITION_RELOAD;
    contents->GetController().LoadURLWithParams(params);
  }
}

Profile* BrowserInstantController::profile() const {
  return browser_->profile();
}
