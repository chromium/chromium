// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/client_tag_hash.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

// Tests that the hashing algorithm has not changed.
TEST(ClientTagHashTest, ShouldGenerateFromUnhashed) {
  EXPECT_EQ("iNFQtRFQb+IZcn1kKUJEZDDkLs4=",
            ClientTagHash::FromUnhashed(PREFERENCES, "tag1").value());
  EXPECT_EQ("gO1cPZQXaM73sHOvSA+tKCKFs58=",
            ClientTagHash::FromUnhashed(AUTOFILL, "tag1").value());

  EXPECT_EQ("XYxkF7bhS4eItStFgiOIAU23swI=",
            ClientTagHash::FromUnhashed(PREFERENCES, "tag2").value());
  EXPECT_EQ("GFiWzo5NGhjLlN+OyCfhy28DJTQ=",
            ClientTagHash::FromUnhashed(AUTOFILL, "tag2").value());
}

}  // namespace syncer
