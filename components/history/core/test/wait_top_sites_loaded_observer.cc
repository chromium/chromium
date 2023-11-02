// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/test/wait_top_sites_loaded_observer.h"

#include "components/history/core/browser/top_sites.h"

namespace history {

WaitTopSitesLoadedObserver::WaitTopSitesLoadedObserver(
    scoped_refptr<TopSites> top_sites)
    : top_sites_(top_sites) {
  if (top_sites_)
    top_sites_->AddObserver(this);
}

WaitTopSitesLoadedObserver::~WaitTopSitesLoadedObserver() {
  if (top_sites_)
    top_sites_->RemoveObserver(this);
}

void WaitTopSitesLoadedObserver::Run() {
  if (top_sites_ && !top_sites_->loaded())
    run_loop_.Run();
}

void WaitTopSitesLoadedObserver::TopSitesLoaded(TopSites* top_sites) {
  run_loop_.Quit();
}

void WaitTopSitesLoadedObserver::TopSitesChanged(TopSites* top_sites,
                                                 ChangeReason change_reason) {
}

}  // namespace history
