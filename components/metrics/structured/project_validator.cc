// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/project_validator.h"

#include <cstdint>

#include "components/metrics/structured/enums.h"

namespace metrics {
namespace structured {

ProjectValidator::ProjectValidator(uint64_t project_hash,
                                   IdType id_type,
                                   IdScope id_scope,
                                   EventType event_type)
    : project_hash_(project_hash),
      id_type_(id_type),
      id_scope_(id_scope),
      event_type_(event_type) {}

ProjectValidator::~ProjectValidator() = default;

}  // namespace structured
}  // namespace metrics
