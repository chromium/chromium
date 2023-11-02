// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_TOPIC_INVALIDATION_MAP_TEST_UTIL_H_
#define COMPONENTS_INVALIDATION_IMPL_TOPIC_INVALIDATION_MAP_TEST_UTIL_H_

// Convince googletest to use the correct overload for PrintTo().
#include "components/invalidation/impl/invalidation_test_util.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace invalidation {

::testing::Matcher<const TopicInvalidationMap&> Eq(
    const TopicInvalidationMap& expected);

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_TOPIC_INVALIDATION_MAP_TEST_UTIL_H_
