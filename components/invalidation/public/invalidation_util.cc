// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/invalidation_util.h"

#include "base/check.h"
#include "components/invalidation/public/invalidation.h"

namespace invalidation {

bool InvalidationVersionLessThan::operator()(const Invalidation& a,
                                             const Invalidation& b) const {
  DCHECK(a.topic() == b.topic()) << "a: " << a.topic() << ", "
                                 << "b: " << b.topic();

  return a.version() < b.version();
}

bool operator==(const TopicMetadata& lhs, const TopicMetadata& rhs) {
  return lhs.is_public == rhs.is_public;
}

}  // namespace invalidation
