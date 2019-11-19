// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_controller.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

class InstantController::TabObserver : public content::WebContentsObserver {
 public:
  TabObserver(content::WebContents* web_contents, const base::Closure& callback)
      : content::WebContentsObserver(web_contents), callback_(callback) {}
  ~TabObserver() override = default;

 private:
  // Overridden from content::WebContentsObserver:
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override {
    if (load_details.is_main_frame && search::IsInstantNTP(web_contents())) {
      callback_.Run();
    }
  }

  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(TabObserver);
};

InstantController::InstantController(Profile* profile,
                                     TabStripModel* tab_strip_model)
    : profile_(profile) {
  tab_strip_model->AddObserver(this);
}

InstantController::~InstantController() = default;

void InstantController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (tab_strip_model->empty() || !selection.active_tab_changed())
    return;

  if (selection.old_contents)
    StopWatchingTab(selection.old_contents);

  if (selection.new_contents)
    StartWatchingTab(selection.new_contents);
}

void InstantController::StartWatchingTab(content::WebContents* web_contents) {
  if (!tab_observer_ || tab_observer_->web_contents() != web_contents) {
    tab_observer_ = std::make_unique<TabObserver>(
        web_contents, base::Bind(&InstantController::UpdateInfoForInstantTab,
                                 base::Unretained(this)));
    // If this tab is an NTP, immediately send it the required info.
    if (search::IsInstantNTP(web_contents)) {
      UpdateInfoForInstantTab();
    }
  }
}

void InstantController::StopWatchingTab(content::WebContents* web_contents) {
  if (tab_observer_ && tab_observer_->web_contents() == web_contents) {
    tab_observer_ = nullptr;
  }
}

void InstantController::UpdateInfoForInstantTab() {
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile_);
  if (instant_service) {
    instant_service->UpdateNtpTheme();
    instant_service->UpdateMostVisitedInfo();
  }
}
