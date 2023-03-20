// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_PREFS_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_PREFS_H_

class PrefRegistrySimple;

namespace history_clusters::prefs {

extern const char kVisible[];
extern const char kShortCache[];
extern const char kAllCache[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace history_clusters::prefs

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_PREFS_H_
