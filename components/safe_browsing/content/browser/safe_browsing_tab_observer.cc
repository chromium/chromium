// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/safe_browsing_tab_observer.h"

#include "base/functional/bind.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "components/safe_browsing/content/browser/client_side_detection_host.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#endif

namespace safe_browsing {

#if !BUILDFLAG(SAFE_BROWSING_AVAILABLE)
// Provide a dummy implementation so that
// std::unique_ptr<ClientSideDetectionHost>
// has a concrete destructor to call. This is necessary because it is used
// as a member of SafeBrowsingTabObserver, even if it only ever contains NULL.
// TODO(shess): This is weird, why not just guard the instance variable?
class ClientSideDetectionHost {};
#endif

SafeBrowsingTabObserver::SafeBrowsingTabObserver(
    content::WebContents* web_contents,
    std::unique_ptr<Delegate> delegate)
    : content::WebContentsUserData<SafeBrowsingTabObserver>(*web_contents),
      delegate_(std::move(delegate)) {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  auto* browser_context = web_contents->GetBrowserContext();
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
          delegate_->CreateClientSideDetectionHost(web_contents);
    }
  }
#endif
}

SafeBrowsingTabObserver::~SafeBrowsingTabObserver() {}

////////////////////////////////////////////////////////////////////////////////
// Internal helpers

void SafeBrowsingTabObserver::UpdateSafebrowsingDetectionHost() {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  auto* browser_context = GetWebContents().GetBrowserContext();
  PrefService* prefs = delegate_->GetPrefs(browser_context);
  bool safe_browsing = IsSafeBrowsingEnabled(*prefs);
  ClientSideDetectionService* csd_service =
      delegate_->GetClientSideDetectionServiceIfExists(browser_context);
  if (safe_browsing && csd_service) {
    if (!safebrowsing_detection_host_.get()) {
      safebrowsing_detection_host_ =
          delegate_->CreateClientSideDetectionHost(&GetWebContents());
    }
  } else {
    safebrowsing_detection_host_.reset();
  }
#endif
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SafeBrowsingTabObserver);

}  // namespace safe_browsing
