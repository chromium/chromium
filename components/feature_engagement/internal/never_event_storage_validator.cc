// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/never_event_storage_validator.h"

namespace feature_engagement {

NeverEventStorageValidator::NeverEventStorageValidator() = default;

NeverEventStorageValidator::~NeverEventStorageValidator() = default;

bool NeverEventStorageValidator::ShouldStore(
    const std::string& event_name) const {
  return false;
}

bool NeverEventStorageValidator::ShouldKeep(const std::string& event_name,
                                            uint32_t event_day,
                                            uint32_t current_day) const {
  return false;
}

}  // namespace feature_engagement
