// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_IOS_PREF_NAMES_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_IOS_PREF_NAMES_H_

// TODO(crbug.com/40768780): Include only prefs needed for iOS.
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/shared_prefs/pref_names.h"

class PrefRegistrySimple;

namespace ios_feed {
void RegisterProfilePrefs(PrefRegistrySimple* registry);
}

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_IOS_PREF_NAMES_H_
