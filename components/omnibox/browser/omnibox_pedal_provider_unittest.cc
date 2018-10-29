// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_pedal_provider.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

class OmniboxPedalProviderTest : public testing::Test {
 protected:
  OmniboxPedalProviderTest() {}
};

TEST_F(OmniboxPedalProviderTest, QueriesTriggerPedals) {
  OmniboxPedalProvider provider;
  EXPECT_EQ(provider.FindPedalMatch(base::ASCIIToUTF16("")), nullptr);
  EXPECT_EQ(provider.FindPedalMatch(base::ASCIIToUTF16("clear histor")),
            nullptr);
  EXPECT_NE(provider.FindPedalMatch(base::ASCIIToUTF16("clear history")),
            nullptr);
}
