// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_PREVIEWS_FEATURES_H_
#define COMPONENTS_PREVIEWS_CORE_PREVIEWS_FEATURES_H_

#include "base/feature_list.h"

namespace previews {
namespace features {

extern const base::Feature kPreviews;
extern const base::Feature kOfflinePreviews;
extern const base::Feature kNoScriptPreviews;
extern const base::Feature kNoScriptPreviewsUserConsistentStudy;
extern const base::Feature kStalePreviewsTimestamp;
extern const base::Feature kResourceLoadingHints;
extern const base::Feature kResourceLoadingHintsUserConsistentStudy;
extern const base::Feature kLitePageServerPreviews;
extern const base::Feature kLitePageServerPreviewsUserConsistentStudy;
extern const base::Feature kSlowPageTriggering;
extern const base::Feature kOfflinePreviewsFalsePositivePrevention;
extern const base::Feature kCoinFlipHoldback;
extern const base::Feature kExcludedMediaSuffixes;
extern const base::Feature kDeferAllScriptPreviews;
extern const base::Feature kDeferAllScriptPreviewsUserConsistentStudy;
extern const base::Feature kEligibleForUserConsistentStudy;

}  // namespace features
}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_FEATURES_H_
