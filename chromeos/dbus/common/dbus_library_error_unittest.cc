// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/common/dbus_library_error.h"

#include <dbus/dbus-protocol.h>

#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

// POD struct for DBusLibraryErrorTest.
struct DBusLibraryErrorTestParams {
  std::string dbus_error_in;
  DBusLibraryError expected_error_out;
};

class DBusLibraryErrorTest
    : public testing::Test,
      public testing::WithParamInterface<DBusLibraryErrorTestParams> {
 public:
  DBusLibraryErrorTest() = default;
  DBusLibraryErrorTest(const DBusLibraryErrorTest&) = delete;
  DBusLibraryErrorTest& operator=(const DBusLibraryErrorTest&) = delete;

  DBusLibraryErrorTestParams params() const { return GetParam(); }
};

TEST_P(DBusLibraryErrorTest, DBusLibraryErrorFromString) {
  DBusLibraryError actual_error_out =
      DBusLibraryErrorFromString(params().dbus_error_in);

  EXPECT_EQ(params().expected_error_out, actual_error_out);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    DBusLibraryErrorTest,
    testing::Values(
        DBusLibraryErrorTestParams{
            /*dbus_error_in=*/DBUS_ERROR_NO_REPLY,
            /*expected_error_out=*/DBusLibraryError::kNoReply},
        DBusLibraryErrorTestParams{
            /*dbus_error_in=*/DBUS_ERROR_TIMEOUT,
            /*expected_error_out=*/DBusLibraryError::kTimeout},
        DBusLibraryErrorTestParams{
            /*dbus_error_in=*/"Nonstandard error message",
            /*expected_error_out=*/DBusLibraryError::kGenericError}));

}  // namespace chromeos
