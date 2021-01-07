// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/external_constants_unittest.h"

#include <memory>

#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/updater_branding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater {

TEST_F(DevOverrideTest, TestDefaults) {
  std::unique_ptr<ExternalConstants> consts = CreateExternalConstants();
  EXPECT_TRUE(consts->UseCUP());
  std::vector<GURL> urls = consts->UpdateURL();
  ASSERT_EQ(urls.size(), 1ul);
  EXPECT_EQ(urls[0], GURL(UPDATE_CHECK_URL));
  EXPECT_TRUE(urls[0].is_valid());
}

}  // namespace updater
