// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_INVALIDATION_CONSTANTS_H_
#define COMPONENTS_INVALIDATION_INVALIDATION_CONSTANTS_H_

namespace invalidation {

// Google Cloud Project number to register with FM
// (go/device-cloud-gcp#fcm-related-projects). Should be used for critical
// invalidations.
inline constexpr char kCriticalInvalidationsProjectNumber[] = "585406161706";

// Google Cloud Project project number to register with FM
// (go/device-cloud-gcp#fcm-related-projects). Should be used for less important
// invalidations.
inline constexpr char kNonCriticalInvalidationsProjectNumber[] = "245350905893";

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_INVALIDATION_CONSTANTS_H_
