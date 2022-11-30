// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/ios/pref_names.h"

#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/shared_prefs/pref_names.h"

namespace ios_feed {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  feed::RegisterProfilePrefs(registry);
  feed::prefs::RegisterFeedSharedProfilePrefs(registry);
}

}  // namespace ios_feed
