// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_MODEL_TYPE_TEST_UTIL_H_
#define COMPONENTS_SYNC_BASE_MODEL_TYPE_TEST_UTIL_H_

#include <ostream>

#include "components/sync/base/model_type.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

// Defined for googletest.  Forwards to ModelTypeSetToDebugString().
void PrintTo(ModelTypeSet model_types, ::std::ostream* os);

// A gmock matcher for ModelTypeSet.  Use like:
//
//   EXPECT_CALL(mock, ProcessModelTypes(HasModelTypes(expected_types)));
::testing::Matcher<ModelTypeSet> HasModelTypes(ModelTypeSet expected_types);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_MODEL_TYPE_TEST_UTIL_H_
