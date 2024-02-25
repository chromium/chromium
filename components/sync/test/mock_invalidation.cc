// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/mock_invalidation.h"

#include "base/check.h"
#include "components/sync/test/mock_invalidation_tracker.h"

namespace syncer {

std::unique_ptr<MockInvalidation> MockInvalidation::BuildUnknownVersion() {
  return std::unique_ptr<MockInvalidation>(
      new MockInvalidation(true, -1, std::string()));
}

std::unique_ptr<MockInvalidation> MockInvalidation::Build(
    int64_t version,
    const std::string& payload) {
  return std::unique_ptr<MockInvalidation>(
      new MockInvalidation(false, version, payload));
}

MockInvalidation::~MockInvalidation() = default;

bool MockInvalidation::IsUnknownVersion() const {
  return is_unknown_version_;
}

const std::string& MockInvalidation::GetPayload() const {
  return payload_;
}

int64_t MockInvalidation::GetVersion() const {
  CHECK(!is_unknown_version_);
  return version_;
}

void MockInvalidation::Acknowledge() {
  // Do nothing.
}

void MockInvalidation::Drop() {
  // Do nothing.
}

MockInvalidation::MockInvalidation(bool is_unknown_version,
                                   int64_t version,
                                   const std::string& payload)
    : is_unknown_version_(is_unknown_version),
      version_(version),
      payload_(payload) {}

}  // namespace syncer
