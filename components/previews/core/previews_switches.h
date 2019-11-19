// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_PREVIEWS_SWITCHES_H_
#define COMPONENTS_PREVIEWS_CORE_PREVIEWS_SWITCHES_H_

namespace previews {
namespace switches {

// Whether the previews blacklist should be ignored, according to command line
// switches.
bool ShouldIgnorePreviewsBlacklist();

extern const char kDoNotRequireLitePageRedirectInfoBar[];
extern const char kIgnorePreviewsBlacklist[];
extern const char kForceEnablePreviews[];
extern const char kLitePageServerPreviewHost[];
extern const char kIgnoreLitePageRedirectOptimizationBlacklist[];
extern const char kClearLitePageRedirectLocalBlacklist[];
extern const char kLitePageRedirectOverridesPageHints[];
extern const char kEnableDeferAllScriptWithoutOptimizationHints[];

}  // namespace switches
}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_SWITCHES_H_
