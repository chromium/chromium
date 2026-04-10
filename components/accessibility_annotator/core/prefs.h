// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_PREFS_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_PREFS_H_

class PrefRegistrySimple;

namespace accessibility_annotator::prefs {

inline constexpr char kShouldShowRemoteAnnotatorFirstRunInfo[] =
    "accessibility_annotator.should_show_remote_annotator_first_run_info";

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace accessibility_annotator::prefs

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_PREFS_H_
