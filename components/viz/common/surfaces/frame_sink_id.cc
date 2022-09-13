// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/frame_sink_id.h"

#include <ostream>

#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"

namespace viz {

std::string FrameSinkId::ToString() const {
  return base::StringPrintf("FrameSinkId(%u, %u)", client_id_, sink_id_);
}

std::string FrameSinkId::ToString(base::StringPiece debug_label) const {
  return base::StringPrintf("FrameSinkId[%s](%u, %u)",
                            std::string(debug_label).c_str(), client_id_,
                            sink_id_);
}

std::ostream& operator<<(std::ostream& out, const FrameSinkId& frame_sink_id) {
  return out << frame_sink_id.ToString();
}

}  // namespace viz
