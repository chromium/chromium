// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Various utilities for dealing with invalidation data types.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_UTIL_H_
#define COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_UTIL_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "components/invalidation/public/invalidation_export.h"

namespace invalidation {

class Invalidation;

struct INVALIDATION_EXPORT InvalidationVersionLessThan {
  bool operator()(const Invalidation& a, const Invalidation& b) const;
};

using Topic = std::string;
// It should be std::set, since std::set_difference is used for it.
using TopicSet = std::set<std::string>;

using TopicCountMap = std::map<Topic, int>;

INVALIDATION_EXPORT struct TopicMetadata {
  // Whether the topic is public.
  bool is_public;
};

INVALIDATION_EXPORT bool operator==(const TopicMetadata&, const TopicMetadata&);

using Topics = std::map<std::string, TopicMetadata>;

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_UTIL_H_
