// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_DATA_TYPE_TEST_UTIL_H_
#define COMPONENTS_SYNC_TEST_DATA_TYPE_TEST_UTIL_H_

#include <ostream>

#include "components/sync/base/data_type.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

// Defined for googletest.  Forwards to DataTypeSetToDebugString().
void PrintTo(DataTypeSet data_types, ::std::ostream* os);

// A gmock matcher for DataTypeSet.  Use like:
//
//   EXPECT_CALL(mock, ProcessDataTypes(HasDataTypes(expected_types)));
::testing::Matcher<DataTypeSet> HasDataTypes(DataTypeSet expected_types);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_DATA_TYPE_TEST_UTIL_H_
