// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_NEVER_EVENT_STORAGE_VALIDATOR_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_NEVER_EVENT_STORAGE_VALIDATOR_H_

#include <string>

#include "components/feature_engagement/internal/event_storage_validator.h"

namespace feature_engagement {

// A EventStorageValidator that never acknowledges that an event should be kept
// or stored.
class NeverEventStorageValidator : public EventStorageValidator {
 public:
  NeverEventStorageValidator();

  NeverEventStorageValidator(const NeverEventStorageValidator&) = delete;
  NeverEventStorageValidator& operator=(const NeverEventStorageValidator&) =
      delete;

  ~NeverEventStorageValidator() override;

  // EventStorageValidator implementation.
  bool ShouldStore(const std::string& event_name) const override;
  bool ShouldKeep(const std::string& event_name,
                  uint32_t event_day,
                  uint32_t current_day) const override;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_NEVER_EVENT_STORAGE_VALIDATOR_H_
