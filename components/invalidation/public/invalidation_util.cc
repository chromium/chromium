// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/invalidation_util.h"

#include <memory>
#include <ostream>
#include <sstream>

#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_handler.h"

namespace invalidation {

bool InvalidationVersionLessThan::operator()(const Invalidation& a,
                                             const Invalidation& b) const {
  DCHECK(a.topic() == b.topic()) << "a: " << a.topic() << ", "
                                 << "b: " << b.topic();

  if (a.is_unknown_version() && !b.is_unknown_version())
    return true;

  if (!a.is_unknown_version() && b.is_unknown_version())
    return false;

  if (a.is_unknown_version() && b.is_unknown_version())
    return false;

  return a.version() < b.version();
}

bool operator==(const TopicMetadata& lhs, const TopicMetadata& rhs) {
  return lhs.is_public == rhs.is_public;
}

}  // namespace invalidation
