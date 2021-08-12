// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/project_validator.h"

#include <cstdint>

namespace metrics {
namespace structured {

ProjectValidator::ProjectValidator(uint64_t project_hash)
    : project_hash_(project_hash) {}

uint64_t ProjectValidator::project_hash() const {
  return project_hash_;
}

}  // namespace structured
}  // namespace metrics
