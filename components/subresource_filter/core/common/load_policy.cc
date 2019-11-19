// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "components/subresource_filter/core/common/load_policy.h"

namespace subresource_filter {

LoadPolicy MoreRestrictiveLoadPolicy(LoadPolicy a, LoadPolicy b) {
  return std::max(a, b);
}

}  // namespace subresource_filter
