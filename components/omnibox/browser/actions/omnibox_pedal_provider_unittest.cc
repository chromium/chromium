// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/omnibox_pedal_provider.h"

#include "base/environment.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/omnibox/browser/actions/omnibox_pedal_concepts.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/common/omnibox_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

class OmniboxPedalProviderTest : public testing::Test {
 protected:
  OmniboxPedalProviderTest() = default;

  void SetUp() override { feature_list_.InitWithFeatures({}, {}); }

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(OmniboxPedalProviderTest, QueriesTriggerPedals) {
  MockAutocompleteProviderClient client;
  std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>> pedals;
  const auto add = [&](OmniboxPedal* pedal) {
    pedals.insert(
        std::make_pair(pedal->PedalId(), base::WrapRefCounted(pedal)));
  };
  add(new TestOmniboxPedalClearBrowsingData());
  client.set_pedal_provider(
      std::make_unique<OmniboxPedalProvider>(client, std::move(pedals)));
  EXPECT_EQ(client.GetPedalProvider()->FindPedalMatch(u""), nullptr);
  EXPECT_EQ(client.GetPedalProvider()->FindPedalMatch(u"clear histor"),
            nullptr);
  EXPECT_NE(client.GetPedalProvider()->FindPedalMatch(u"clear history"),
            nullptr);
}
