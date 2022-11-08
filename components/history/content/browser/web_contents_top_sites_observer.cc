// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/content/browser/web_contents_top_sites_observer.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "components/history/core/browser/top_sites.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"

namespace history {

WebContentsTopSitesObserver::WebContentsTopSitesObserver(
    content::WebContents* web_contents,
    TopSites* top_sites)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<WebContentsTopSitesObserver>(*web_contents),
      top_sites_(top_sites) {}

WebContentsTopSitesObserver::~WebContentsTopSitesObserver() {
}

void WebContentsTopSitesObserver::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  DCHECK(load_details.entry);

  // We only care about navigating the main frame.
  if (top_sites_ && load_details.is_main_frame) {
    // Only report the Virtual URL. The virtual URL, when it differs from the
    // actual URL that is loaded in the renderer, is the one meant to be shown
    // to the user in all UI.
    top_sites_->OnNavigationCommitted(load_details.entry->GetVirtualURL());
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsTopSitesObserver);

}  // namespace history
