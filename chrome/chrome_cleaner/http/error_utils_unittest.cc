// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/http/error_utils.h"

#include <ostream>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace common {

TEST(ErrorUtils, HrLog) {
  {
    std::ostringstream stream;
    stream << LogHr(S_OK);
    std::string str = stream.str();
    EXPECT_NE(str.find("0x0,"), std::string::npos);
    EXPECT_NE(str.find("msg="), std::string::npos);
  }

  {
    std::ostringstream stream;
    stream << LogHr(E_FAIL);
    std::string str = stream.str();
    EXPECT_NE(str.find("0x80004005,"), std::string::npos);
    EXPECT_NE(str.find("msg=Unspecified error"), std::string::npos);
  }
}

TEST(ErrorUtils, WeLog) {
  {
    std::ostringstream stream;
    stream << LogWe(ERROR_SUCCESS);
    std::string str = stream.str();
    EXPECT_NE(str.find("[we=0,"), std::string::npos);
    EXPECT_NE(str.find("msg=The operation completed successfully"),
              std::string::npos);
  }

  {
    std::ostringstream stream;
    stream << LogWe(ERROR_INVALID_FUNCTION);
    std::string str = stream.str();
    EXPECT_NE(str.find("[we=1,"), std::string::npos);
    EXPECT_NE(str.find("msg=Incorrect function"), std::string::npos);
  }
}

}  // namespace common
