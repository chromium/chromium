// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_TEST_WAIT_TOP_SITES_LOADED_OBSERVER_H_
#define COMPONENTS_HISTORY_CORE_TEST_WAIT_TOP_SITES_LOADED_OBSERVER_H_

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "components/history/core/browser/top_sites_observer.h"

namespace history {

class TopSites;

// Used to make sure TopSites has finished loading
class WaitTopSitesLoadedObserver : public TopSitesObserver {
 public:
  explicit WaitTopSitesLoadedObserver(scoped_refptr<TopSites> top_sites);

  WaitTopSitesLoadedObserver(const WaitTopSitesLoadedObserver&) = delete;
  WaitTopSitesLoadedObserver& operator=(const WaitTopSitesLoadedObserver&) =
      delete;

  ~WaitTopSitesLoadedObserver() override;

  // Wait until TopSites has finished loading. Returns immediately if it has
  // already been loaded.
  void Run();

 private:
  // TopSitesObserver implementation.
  void TopSitesLoaded(TopSites* top_sites) override;
  void TopSitesChanged(TopSites* top_sites,
                       ChangeReason change_reason) override;

  scoped_refptr<TopSites> top_sites_;
  base::RunLoop run_loop_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_TEST_WAIT_TOP_SITES_LOADED_OBSERVER_H_
