// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_pedal_provider.h"

#include "base/environment.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

class OmniboxPedalProviderTest : public testing::Test {
 protected:
  OmniboxPedalProviderTest() {}
};

TEST_F(OmniboxPedalProviderTest, QueriesTriggerPedals) {
  MockAutocompleteProviderClient client;
  AutocompleteInput input;
  OmniboxPedalProvider provider(client);
  EXPECT_EQ(provider.FindPedalMatch(input, base::ASCIIToUTF16("")), nullptr);
  EXPECT_EQ(provider.FindPedalMatch(input, base::ASCIIToUTF16("clear histor")),
            nullptr);
  EXPECT_NE(provider.FindPedalMatch(input, base::ASCIIToUTF16("clear history")),
            nullptr);
}
