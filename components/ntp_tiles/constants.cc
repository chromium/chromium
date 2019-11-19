// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/constants.h"

namespace ntp_tiles {

const size_t kMaxNumCustomLinks = 10;

// Because the custom links feature has an additional "Add shortcut" button, 9
// tiles are required to balance the Most Visited rows on the NTP.
const int kMaxNumMostVisited = 9;

}  // namespace ntp_tiles
