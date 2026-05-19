// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/stream_type.h"

namespace feed {

std::string StreamType::ToString() const {
  switch (kind_) {
    case StreamKind::kUnknown:
      return "Unknown";
    case StreamKind::kForYou:
      return "ForYou";
  }
}

}  // namespace feed
