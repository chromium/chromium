// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_OAUTH_CONSUMER_IDS_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_OAUTH_CONSUMER_IDS_H_

namespace signin {

// LINT.IfChange(OAuthConsumerId)
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OAuthConsumerId {
  kSync = 0,
  kWallpaperGooglePhotosFetcher = 1,
  kWallpaperFetcherDelegate = 2,
  kIpProtectionService = 3,
  kSanitizedImageSource = 4,
  kOptimizationGuideGetHints = 5,
  kOptimizationGuideModelExecution = 6,
  kMaxValue = kOptimizationGuideModelExecution,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:OAuthConsumerId)

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_OAUTH_CONSUMER_IDS_H_
