// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CONTENT_BROWSER_WEB_CONTENTS_TOP_SITES_OBSERVER_H_
#define COMPONENTS_HISTORY_CONTENT_BROWSER_WEB_CONTENTS_TOP_SITES_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace history {

class TopSites;

// WebContentsTopSitesObserver forwards navigation events from
// content::WebContents to TopSites.
class WebContentsTopSitesObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WebContentsTopSitesObserver> {
 public:
  WebContentsTopSitesObserver(const WebContentsTopSitesObserver&) = delete;
  WebContentsTopSitesObserver& operator=(const WebContentsTopSitesObserver&) =
      delete;

  ~WebContentsTopSitesObserver() override;

 private:
  friend class content::WebContentsUserData<WebContentsTopSitesObserver>;
  friend class WebContentsTopSitesObserverTest;

  WebContentsTopSitesObserver(content::WebContents* web_contents,
                              TopSites* top_sites);

  // content::WebContentsObserver implementation.
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;

  // Underlying TopSites instance, may be null during testing.
  raw_ptr<TopSites, DanglingUntriaged> top_sites_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CONTENT_BROWSER_WEB_CONTENTS_TOP_SITES_OBSERVER_H_
