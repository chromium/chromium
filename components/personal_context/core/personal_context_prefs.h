// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_PREFS_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_PREFS_H_

class PrefRegistrySimple;

namespace personal_context::prefs {

inline constexpr char kShouldShowPersonalContextFirstRunInfo[] =
    "personal_context.should_show_first_run_info";

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace personal_context::prefs

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_PREFS_H_
