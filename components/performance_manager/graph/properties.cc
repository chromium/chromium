// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/properties.h"

namespace performance_manager {

perfetto::StaticString YesNoStateToString(const bool& is_yes) {
  if (is_yes) {
    return "yes";
  } else {
    return "no";
  }
}

}  // namespace performance_manager
