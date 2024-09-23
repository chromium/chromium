// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/variations/hashing.h"

#include <stddef.h>
#include <stdint.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

TEST(HashingTest, HashName) {
  // Checks that hashing is stable on all platforms.
  struct {
    const char* name;
    uint32_t hash_value;
  } known_hashes[] = {{"a", 937752454u},
                      {"1", 723085877u},
                      {"Trial Name", 2713117220u},
                      {"Group Name", 3201815843u},
                      {"My Favorite Experiment", 3722155194u},
                      {"My Awesome Group Name", 4109503236u},
                      {"abcdefghijklmonpqrstuvwxyz", 787728696u},
                      {"0123456789ABCDEF", 348858318U}};

  for (const auto& hash_case : known_hashes) {
    EXPECT_EQ(hash_case.hash_value, HashName(hash_case.name));
  }
}

TEST(HashingTest, HashNameAsHexString) {
  EXPECT_EQ("37e4f786", HashNameAsHexString("a"));
}

}  // namespace variations
