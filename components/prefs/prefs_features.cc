// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/prefs_features.h"

// Serialize the pref store to JSON on a background sequence instead of the main
// thread.
BASE_FEATURE(kPrefStoreBackgroundSerialization,
             "PrefStoreBackgroundSerialization",
             base::FEATURE_ENABLED_BY_DEFAULT);
