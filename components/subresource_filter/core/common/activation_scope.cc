// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/activation_scope.h"

#include <ostream>

#include "base/notreached.h"

namespace subresource_filter {

std::ostream& operator<<(std::ostream& os, const ActivationScope& state) {
  switch (state) {
    case ActivationScope::NO_SITES:
      os << "NO_SITES";
      break;
    case ActivationScope::ALL_SITES:
      os << "ALL_SITES";
      break;
    case ActivationScope::ACTIVATION_LIST:
      os << "ACTIVATION_LIST";
      break;
    default:
      NOTREACHED();
      break;
  }
  return os;
}

}  // namespace subresource_filter
