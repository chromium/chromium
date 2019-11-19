// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_start_params.h"

namespace arc {

StartParams::StartParams() = default;
StartParams::~StartParams() = default;
StartParams::StartParams(StartParams&& other) = default;
StartParams& StartParams::operator=(StartParams&& other) = default;

}  // namespace arc
