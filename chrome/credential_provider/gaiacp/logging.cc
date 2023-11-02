// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/logging.h"

#include <iomanip>

namespace credential_provider {

std::ostream& operator<<(std::ostream& stream, const putHR& o) {
  stream << std::setbase(16) << o.hr_;
  return stream;
}

}  // namespace credential_provider
