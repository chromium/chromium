// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/switches.h"

namespace feature_engagement {
namespace switches {

// This switch causes TrackerImpl to be wrapped, so that functions can be
// proxied up into Java land, to facilitate end-to-end Java tests that exercise
// the feature engagement.
const char kUseJavaProxyTracker[] = "use-java-proxy-tracker";

}  // namespace switches
}  // namespace feature_engagement