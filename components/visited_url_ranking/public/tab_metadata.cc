// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/public/tab_metadata.h"

namespace visited_url_ranking {

TabMetadata::TabMetadata() = default;
TabMetadata::~TabMetadata() = default;

TabMetadata::TabMetadata(const TabMetadata&) = default;
TabMetadata& TabMetadata::operator=(const TabMetadata&) = default;
}  // namespace visited_url_ranking
