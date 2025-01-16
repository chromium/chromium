// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "enterprise_search_aggregator_provider.h"

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/page_transition_types.h"

class FakeEnterpriseSearchAggregatorProvider
    : public EnterpriseSearchAggregatorProvider {
 public:
  using EnterpriseSearchAggregatorProvider::CreateMatch;
  using EnterpriseSearchAggregatorProvider::EnterpriseSearchAggregatorProvider;

 protected:
  ~FakeEnterpriseSearchAggregatorProvider() override = default;
};

class EnterpriseSearchAggregatorProviderTest : public testing::Test {
 protected:
  EnterpriseSearchAggregatorProviderTest() {
    client_ = std::make_unique<FakeAutocompleteProviderClient>();
    provider_ = new FakeEnterpriseSearchAggregatorProvider(client_.get());
  }

  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<FakeEnterpriseSearchAggregatorProvider> provider_;
};

TEST_F(EnterpriseSearchAggregatorProviderTest, CreateMatch) {
  AutocompleteInput input{u"input text", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier()};
  input.set_keyword_mode_entry_method(metrics::OmniboxEventProto::TAB);

  auto match =
      provider_->CreateMatch(input, u"keyword", true, 1000, "https://url.com",
                             u"title", u"additional text");
  EXPECT_EQ(match.destination_url.spec(), "https://url.com/");
  EXPECT_EQ(match.fill_into_edit, u"https://url.com");
  EXPECT_EQ(match.description, u"title");
  EXPECT_EQ(match.contents, u"additional text");
  EXPECT_EQ(match.keyword, u"keyword");
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(match.transition, ui::PAGE_TRANSITION_KEYWORD));
  EXPECT_EQ(match.from_keyword, true);
}
