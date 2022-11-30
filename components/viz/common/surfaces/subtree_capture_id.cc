// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/subtree_capture_id.h"

#include "base/strings/stringprintf.h"

namespace viz {

std::string SubtreeCaptureId::ToString() const {
  return base::StringPrintf("SubtreeCaptureId(%u)", subtree_id_);
}

}  // namespace viz
