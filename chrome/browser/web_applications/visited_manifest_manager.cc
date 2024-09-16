// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/visited_manifest_manager.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/origin.h"

namespace web_app {

// TODO(crbug.com/366323698): Clean this up after m131 hits stable.
BASE_FEATURE(kBlockMlPromotionInNestedPagesNoManifest,
             "BlockMlPromotionInNestedPagesNoManifest",
             base::FEATURE_ENABLED_BY_DEFAULT);

VisitedManifestManager::VisitedManifestManager(base::Clock* clock)
    : clock_(clock) {}
VisitedManifestManager::~VisitedManifestManager() = default;

bool VisitedManifestManager::IsUrlControlledBySeenManifest(
    const GURL& site_url) {
  if (!base::FeatureList::IsEnabled(kBlockMlPromotionInNestedPagesNoManifest)) {
    return false;
  }
  // This returns the first value 'greater than' the given value. Prefixes MUST
  // be equal or less than the candidate string, so the algorithm can iterate
  // backwards until it reaches a different origin.
  auto index = recent_manifest_scopes_.index();
  auto it =
      std::ranges::upper_bound(index, site_url, std::less{},
                               [](const auto& pair) { return pair.first; });

  url::Origin site_origin = url::Origin::Create(site_url);
  while (it != index.begin()) {
    --it;
    if (base::StartsWith(site_url.spec(), it->first.spec())) {
      recent_manifest_scopes_.Put(it->first, clock_->Now());
      return true;
    }
    if (!site_origin.IsSameOriginWith(it->first)) {
      return false;
    }
  }
  return false;
}

void VisitedManifestManager::OnManifestSeen(
    const blink::mojom::Manifest& manifest) {
  if (!base::FeatureList::IsEnabled(kBlockMlPromotionInNestedPagesNoManifest)) {
    return;
  }
  CHECK(!blink::IsEmptyManifest(manifest));
  // Only consider manifests that have specified a `start_url`, as every page
  // has a default manifest.
  if (!manifest.has_valid_specified_start_url) {
    return;
  }
  recent_manifest_scopes_.Put(manifest.scope, clock_->Now());
}

void VisitedManifestManager::ClearSeenScopes(const base::Time& begin_time,
                                             const base::Time& end_time) {
  if (!base::FeatureList::IsEnabled(kBlockMlPromotionInNestedPagesNoManifest)) {
    return;
  }
  auto it = recent_manifest_scopes_.begin();
  while (it != recent_manifest_scopes_.end()) {
    if (it->second >= begin_time && it->second <= end_time) {
      it = recent_manifest_scopes_.Erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace web_app
