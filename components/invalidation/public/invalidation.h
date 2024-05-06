// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_H_
#define COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_H_

#include <stdint.h>

#include <string>

#include "components/invalidation/public/invalidation_export.h"
#include "components/invalidation/public/invalidation_util.h"

namespace invalidation {

// Represents a local invalidation.
class INVALIDATION_EXPORT Invalidation {
 public:
  Invalidation(const Topic& topic, int64_t version, const std::string& payload);
  Invalidation(const Invalidation& other);
  Invalidation& operator=(const Invalidation& other);
  ~Invalidation();

  // Compares two invalidations.
  bool operator==(const Invalidation& other) const;

  Topic topic() const;
  int64_t version() const;
  const std::string& payload() const;

 private:
  // The Topic to which this invalidation belongs.
  Topic topic_;

  // The version number of this invalidation.
  int64_t version_;

  // The payload associated with this invalidation.
  std::string payload_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_H_
