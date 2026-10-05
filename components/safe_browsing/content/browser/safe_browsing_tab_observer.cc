// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/safe_browsing_tab_observer.h"

#include "base/functional/bind.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/client_side_detection_host.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace safe_browsing {

DEFINE_USER_DATA(SafeBrowsingTabObserver);

SafeBrowsingTabObserver::SafeBrowsingTabObserver(
    tabs::TabInterface& tab,
    content::WebContents* web_contents,
    std::unique_ptr<Delegate> delegate)
    : web_contents_(web_contents),
      delegate_(std::move(delegate)),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  auto* browser_context = web_contents_->GetBrowserContext();
  PrefService* prefs = delegate_->GetPrefs(browser_context);
  if (prefs) {
    pref_change_registrar_.Init(prefs);
    pref_change_registrar_.Add(
        prefs::kSafeBrowsingEnabled,
        base::BindRepeating(
            &SafeBrowsingTabObserver::UpdateSafebrowsingDetectionHost,
            base::Unretained(this)));

    ClientSideDetectionService* csd_service =
        delegate_->GetClientSideDetectionServiceIfExists(browser_context);
    if (IsSafeBrowsingEnabled(*prefs) &&
        delegate_->DoesSafeBrowsingServiceExist() && csd_service) {
      safebrowsing_detection_host_ =
          delegate_->CreateClientSideDetectionHost(web_contents_);
    }
  }
}

SafeBrowsingTabObserver::~SafeBrowsingTabObserver() = default;

// static
SafeBrowsingTabObserver* SafeBrowsingTabObserver::From(
    tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

// static
SafeBrowsingTabObserver* SafeBrowsingTabObserver::FromWebContents(
    content::WebContents* web_contents) {
  return web_contents
             ? From(tabs::TabInterface::MaybeGetFromContents(web_contents))
             : nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// Internal helpers

void SafeBrowsingTabObserver::UpdateSafebrowsingDetectionHost() {
  auto* browser_context = web_contents_->GetBrowserContext();
  PrefService* prefs = delegate_->GetPrefs(browser_context);
  bool safe_browsing = IsSafeBrowsingEnabled(*prefs);
  ClientSideDetectionService* csd_service =
      delegate_->GetClientSideDetectionServiceIfExists(browser_context);
  if (safe_browsing && csd_service) {
    if (!safebrowsing_detection_host_.get()) {
      safebrowsing_detection_host_ =
          delegate_->CreateClientSideDetectionHost(web_contents_);
    }
  } else {
    safebrowsing_detection_host_.reset();
  }
}

}  // namespace safe_browsing
