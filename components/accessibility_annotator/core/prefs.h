// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_PREFS_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_PREFS_H_

class PrefRegistrySimple;

namespace accessibility_annotator::prefs {

inline constexpr char kUkmLoggingUserSecret[] =
    "accessibility_annotator.ukm_logging_user_secret";

inline constexpr char kUkmLoggingUserSecretCreationTime[] =
    "accessibility_annotator.ukm_logging_user_secret_creation_time";

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace accessibility_annotator::prefs

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_PREFS_H_
