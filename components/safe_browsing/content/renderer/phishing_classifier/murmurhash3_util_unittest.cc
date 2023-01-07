// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit test to verify basic operation of murmurhash3.

#include "components/safe_browsing/content/renderer/phishing_classifier/murmurhash3_util.h"

#include <string>
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

TEST(MurmurHash3UtilTest, MurmurHash3String) {
  EXPECT_EQ(893017187U, MurmurHash3String("abcd", 1234U));
  EXPECT_EQ(3322282861U, MurmurHash3String("abcde", 56789U));
}

}  // namespace safe_browsing
