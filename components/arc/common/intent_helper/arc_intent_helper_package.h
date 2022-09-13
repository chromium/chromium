// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMMON_INTENT_HELPER_ARC_INTENT_HELPER_PACKAGE_H_
#define COMPONENTS_ARC_COMMON_INTENT_HELPER_ARC_INTENT_HELPER_PACKAGE_H_

namespace arc {

// The name of intent helper package which is used by ARC to send URLs to
// Chrome.
// This package does not count as a candidate to open ARC apps, so it should be
// removed from candidate list such as context menu.
constexpr char kArcIntentHelperPackageName[] = "org.chromium.arc.intent_helper";

}  // namespace arc

#endif  // COMPONENTS_ARC_COMMON_INTENT_HELPER_ARC_INTENT_HELPER_PACKAGE_H_
