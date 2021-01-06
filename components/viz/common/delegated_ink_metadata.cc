// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>

#include "base/strings/stringprintf.h"
#include "components/viz/common/delegated_ink_metadata.h"

namespace viz {

std::string DelegatedInkMetadata::ToString() const {
  std::string str = base::StringPrintf(
      "point: %s, diameter: %f, color: %u, timestamp: %" PRId64
      ", presentation_area: %s, frame_time: %" PRId64 ", is_hovering: %d",
      point_.ToString().c_str(), diameter_, color_,
      timestamp_.since_origin().InMicroseconds(),
      presentation_area_.ToString().c_str(),
      frame_time_.since_origin().InMicroseconds(), is_hovering_);
  return str;
}

}  // namespace viz
