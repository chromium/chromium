// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/tab_group_provider.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

class TabGroupProviderTest : public testing::Test {
 public:
  void SetUp() override {
    tab_group_provider_ = new TabGroupProvider(&client_);
  }

  FakeAutocompleteProviderClient& client() { return client_; }

  TabGroupProvider& tab_group_provider() { return *tab_group_provider_; }

  tab_groups::SavedTabGroup CreateSavedTabGroup() {
    const base::Uuid kGroupGuid = base::Uuid::GenerateRandomV4();

    // Add a saved tab group locally and simulate a remote creation of a shared
    // tab group with the same GUID.
    tab_groups::SavedTabGroup saved_group(
        u"test", tab_groups::TabGroupColorId::kGrey,
        /*urls=*/{}, /*position=*/0, kGroupGuid);
    tab_groups::SavedTabGroupTab tab_1(GURL("http://google.com/saved_1"),
                                       u"Saved tab 1", kGroupGuid,
                                       /*position=*/0);
    tab_groups::SavedTabGroupTab tab_2(GURL("http://google.com/saved_2"),
                                       u"Saved tab 2", kGroupGuid,
                                       /*position=*/1);
    saved_group.AddTabLocally(tab_1);
    saved_group.AddTabLocally(tab_2);
    return saved_group;
  }

 private:
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  base::test::TaskEnvironment task_environment_;
  FakeAutocompleteProviderClient client_;
  scoped_refptr<TabGroupProvider> tab_group_provider_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(TabGroupProviderTest, TestNoResults) {
  AutocompleteInput input(u"test",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  tab_group_provider().Start(input, /* minimal_changes= */ false);
  ASSERT_EQ(0UL, tab_group_provider().matches().size());
}

TEST_F(TabGroupProviderTest, TestOneTitleMatch) {
  tab_groups::FakeTabGroupSyncService* service =
      static_cast<tab_groups::FakeTabGroupSyncService*>(
          client().GetTabGroupSyncService());
  service->AddGroup(CreateSavedTabGroup());

  AutocompleteInput input(u"test",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  tab_group_provider().Start(input, /* minimal_changes= */ false);
  ASSERT_EQ(1UL, tab_group_provider().matches().size());
  ASSERT_EQ(u"test", tab_group_provider().matches()[0].contents);
  ASSERT_TRUE(
      tab_group_provider().matches()[0].matching_tab_group_uuid.has_value());
}
