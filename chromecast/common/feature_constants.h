// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_FEATURE_CONSTANTS_H_
#define CHROMECAST_COMMON_FEATURE_CONSTANTS_H_

namespace chromecast {
namespace feature {

// TODO(b/187758538): Upstream more Feature Constants here.

// TODO(b/187524799): Remove this feature when the related features are
// deprecated.
extern const char kEnableTrackControlAppRendererFeatureUse[];
// The app can use playready.
extern const char kEnablePlayready[];

extern const char kKeyAppId[];
// Insecure content is allowed for the app.
extern const char kKeyAllowInsecureContent[];

}  // namespace feature
}  // namespace chromecast

#endif  // CHROMECAST_COMMON_FEATURE_CONSTANTS_H_
