// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_SYSTEM_FEATURES_DISABLE_LIST_CONSTANTS_H_
#define COMPONENTS_POLICY_CORE_COMMON_SYSTEM_FEATURES_DISABLE_LIST_CONSTANTS_H_

namespace policy {

// Modes for the SystemFeaturesDisableMode policy. This policy controls the
// visibility of features and apps disabled by the SystemFeaturesDisableList
// policy.

// "blocked": Disabled features and apps are unusable but still visible to the
// user (grayed out).
static inline constexpr char kSystemFeaturesDisableModeBlocked[] = "blocked";

// "hidden": Disabled features and apps are unusable and invisible to the users.
static inline constexpr char kSystemFeaturesDisableModeHidden[] = "hidden";

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_SYSTEM_FEATURES_DISABLE_LIST_CONSTANTS_H_
