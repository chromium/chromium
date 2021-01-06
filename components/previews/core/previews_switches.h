// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_PREVIEWS_SWITCHES_H_
#define COMPONENTS_PREVIEWS_CORE_PREVIEWS_SWITCHES_H_

namespace previews {
namespace switches {

// Whether the previews blocklist should be ignored, according to command line
// switches.
bool ShouldIgnorePreviewsBlocklist();

extern const char kDoNotRequireLitePageRedirectInfoBar[];
extern const char kIgnorePreviewsBlocklist[];
extern const char kEnableDeferAllScriptWithoutOptimizationHints[];

}  // namespace switches
}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_SWITCHES_H_
