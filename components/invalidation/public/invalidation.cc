// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/invalidation.h"

#include <string>

#include "components/invalidation/public/invalidation_util.h"

namespace invalidation {

Invalidation::Invalidation(const Topic& topic,
                           int64_t version,
                           const std::string& payload)
    : topic_(topic), version_(version), payload_(payload) {}

Invalidation::Invalidation(const Invalidation& other) = default;

Invalidation& Invalidation::operator=(const Invalidation& other) = default;

Invalidation::~Invalidation() = default;

Topic Invalidation::topic() const {
  return topic_;
}

int64_t Invalidation::version() const {
  return version_;
}

const std::string& Invalidation::payload() const {
  return payload_;
}

bool Invalidation::operator==(const Invalidation& other) const = default;

}  // namespace invalidation
