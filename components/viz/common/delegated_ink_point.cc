// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/delegated_ink_point.h"

#include <inttypes.h>

#include "base/strings/stringprintf.h"

namespace viz {

std::string DelegatedInkPoint::ToString() const {
  return base::StringPrintf("point: %s, timestamp: %" PRId64 ", pointer_id: %d",
                            point_.ToString().c_str(),
                            timestamp_.since_origin().InMicroseconds(),
                            pointer_id_);
}

}  // namespace viz
