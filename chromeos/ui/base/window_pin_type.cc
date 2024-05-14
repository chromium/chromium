// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/base/window_pin_type.h"

#include "base/notreached.h"

namespace chromeos {

std::ostream& operator<<(std::ostream& out, WindowPinType pin_type) {
  switch (pin_type) {
    case WindowPinType::kNone:
      return out << "kNone";
    case WindowPinType::kPinned:
      return out << "kPinned";
    case WindowPinType::kTrustedPinned:
      return out << "kTrustedPinned";
  }

  NOTREACHED_IN_MIGRATION();
  return out;
}

}  // namespace chromeos
