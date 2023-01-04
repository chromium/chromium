// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/common/power_overview.h"

#include "components/power_bookmarks/common/power.h"

namespace power_bookmarks {

PowerOverview::PowerOverview(std::unique_ptr<Power> power, size_t count)
    : power_(std::move(power)), count_(count) {}

PowerOverview::~PowerOverview() = default;

}  // namespace power_bookmarks