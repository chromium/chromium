// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/aggregated_frame.h"

namespace viz {

AggregatedFrame::AggregatedFrame() = default;
AggregatedFrame::AggregatedFrame(AggregatedFrame&& other) = default;
AggregatedFrame::~AggregatedFrame() = default;

AggregatedFrame& AggregatedFrame::operator=(AggregatedFrame&& other) = default;

}  // namespace viz
