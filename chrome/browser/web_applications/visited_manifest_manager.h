// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_VISITED_MANIFEST_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_VISITED_MANIFEST_MANAGER_H_

#include "base/containers/lru_cache.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "url/gurl.h"

namespace web_app {

BASE_DECLARE_FEATURE(kBlockMlPromotionInNestedPagesNoManifest);

// This class is used to track recently seen manifests. Currently this is
// limited only to manifest scopes, which are used to detect if a given url
// is in-scope a recently seen manifest entity. This class limits its history
// to `kMaxScopesToSave` items, which are evicted via the least-recently-used
// policy.
// Seeing a manifest scope AND detecting if a scope controls a url both 'update'
// that entry to be at the front of the LRU queue.
class VisitedManifestManager {
 public:
  static constexpr int kMaxScopesToSave = 100;

  explicit VisitedManifestManager(
      base::Clock* clock = base::DefaultClock::GetInstance());
  ~VisitedManifestManager();

  // If true, then this will also update the scope entry to be at the front of
  // the FIFO queue, and record a new time for use with `ClearSeenScopes`.
  bool IsUrlControlledBySeenManifest(const GURL& url);

  // Records that the given manifest has been seen now. Only scopes of manifests
  // that specify a `start_url` will be saved, and the current time will be
  // recorded for use with `ClearSeenScopes`. Called when a manifest scope is
  // seen by the AppBannerManager.
  void OnManifestSeen(const blink::mojom::Manifest& manifest);

  // Clears scopes that have been recorded in the given time, to be called from
  // the clear-browsing-data flow.
  void ClearSeenScopes(const base::Time& begin_time,
                       const base::Time& end_time);

 private:
  base::LRUCache<GURL /*scope*/, base::Time /*last_seen_or_used*/>
      recent_manifest_scopes_{kMaxScopesToSave};

  raw_ptr<base::Clock> clock_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_VISITED_MANIFEST_MANAGER_H_
