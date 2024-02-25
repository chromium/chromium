// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Various utilities for dealing with invalidation data types.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_UTIL_H_
#define COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_UTIL_H_

#include <map>

#include <set>
#include <string>

#include "components/invalidation/public/invalidation_export.h"

namespace invalidation {

using Topic = std::string;
// It should be std::set, since std::set_difference is used for it.
using TopicSet = std::set<Topic>;

INVALIDATION_EXPORT struct TopicMetadata {
  // Whether the topic is public.
  bool is_public;
};

INVALIDATION_EXPORT bool operator==(const TopicMetadata&, const TopicMetadata&);

using TopicMap = std::map<Topic, TopicMetadata>;

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_UTIL_H_
