// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/open_tab_provider.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

class OpenTabProviderTest : public testing::Test {
 public:
  OpenTabProviderTest() { open_tab_provider_ = new OpenTabProvider(&client_); }

  FakeAutocompleteProviderClient& client() { return client_; }

  OpenTabProvider& open_tab_provider() { return *open_tab_provider_; }

 private:
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  base::test::TaskEnvironment task_environment_;
  FakeAutocompleteProviderClient client_;
  scoped_refptr<OpenTabProvider> open_tab_provider_;
};

TEST_F(OpenTabProviderTest, TestNoResults) {
  AutocompleteInput input(u"test",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  open_tab_provider().Start(input, /* minimal_changes= */ false);
  ASSERT_EQ(0UL, open_tab_provider().matches().size());
}

TEST_F(OpenTabProviderTest, TestTitleMatch) {
  TabMatcher::TabWrapper open_tab(u"test title", GURL("http://google.com"));
  static_cast<FakeTabMatcher&>(
      const_cast<TabMatcher&>(client().GetTabMatcher()))
      .AddOpenTab(open_tab);

  AutocompleteInput input(u"test",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  open_tab_provider().Start(input, /* minimal_changes= */ false);
  ASSERT_EQ(1UL, open_tab_provider().matches().size());
}

TEST_F(OpenTabProviderTest, TestURLMatch) {
  TabMatcher::TabWrapper open_tab(u"google", GURL("http://test.com"));
  static_cast<FakeTabMatcher&>(
      const_cast<TabMatcher&>(client().GetTabMatcher()))
      .AddOpenTab(open_tab);

  AutocompleteInput input(u"test",
                          metrics::OmniboxEventProto::PageClassification::
                              OmniboxEventProto_PageClassification_ANDROID_HUB,
                          TestSchemeClassifier());
  open_tab_provider().Start(input, /* minimal_changes= */ false);
  ASSERT_EQ(1UL, open_tab_provider().matches().size());
}
