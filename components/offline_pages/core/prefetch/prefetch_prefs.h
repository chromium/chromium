// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_PREFS_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_PREFS_H_

class PrefRegistrySimple;

namespace offline_pages {

namespace prefetch_prefs {

extern const char kBackoff[];
extern const char kUserSettingEnabled[];

void RegisterPrefs(PrefRegistrySimple* registry);

}  // namespace prefetch_prefs
}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_PREFS_H_
