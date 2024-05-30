
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_SWITCHES_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_SWITCHES_H_

namespace switches {

// Specifies the allowed IPH features; if there are no arguments, then no IPH
// features are allowed, effectively disabling IPH.
inline constexpr char kPropagateIPHForTesting[] = "propagate-iph-for-testing";

}  // namespace switches

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_SWITCHES_H_
