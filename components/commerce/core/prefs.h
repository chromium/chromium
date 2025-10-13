// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PREFS_H_
#define COMPONENTS_COMMERCE_CORE_PREFS_H_

class PrefRegistrySimple;

namespace commerce {

// Register preference names for commerce features.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PREFS_H_
