// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_instant_controller.h"

#include "base/bind.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"

// Helpers --------------------------------------------------------------------

namespace {

// Helper class for posting a task to reload a tab, to avoid doing a re-entrant
// navigation, since it can be called when starting a navigation. This class
// makes sure to only execute the reload if the WebContents still exists.
class TabReloader : public content::WebContentsUserData<TabReloader> {
 public:
  ~TabReloader() override {}

  static void Reload(content::WebContents* web_contents) {
    TabReloader::CreateForWebContents(web_contents);
  }

 private:
  friend class content::WebContentsUserData<TabReloader>;

  explicit TabReloader(content::WebContents* web_contents)
      : web_contents_(web_contents) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&TabReloader::ReloadImpl,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  void ReloadImpl() {
    web_contents_->GetController().Reload(content::ReloadType::NORMAL, false);

    // As the reload was not triggered by the user we don't want to close any
    // infobars. We have to tell the InfoBarService after the reload,
    // otherwise it would ignore this call when
    // WebContentsObserver::DidStartNavigationToPendingEntry is invoked.
    InfoBarService::FromWebContents(web_contents_)->set_ignore_next_reload();

    web_contents_->RemoveUserData(UserDataKey());
  }

  content::WebContents* web_contents_;
  base::WeakPtrFactory<TabReloader> weak_ptr_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabReloader)

}  // namespace

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

    GURL site_url = contents->GetMainFrame()->GetSiteInstance()->GetSiteURL();
    bool is_ntp = site_url == GURL(chrome::kChromeUINewTabPageURL) ||
                  site_url == GURL(chrome::kChromeUINewTabPageThirdPartyURL);

    if (!is_ntp) {
      InstantService* instant_service =
          InstantServiceFactory::GetForProfile(profile());
      if (instant_service) {
        content::RenderProcessHost* rph =
            contents->GetMainFrame()->GetProcess();
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
