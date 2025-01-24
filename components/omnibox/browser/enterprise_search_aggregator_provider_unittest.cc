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
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/page_transition_types.h"

namespace {
using testing::Return;
}  // namespace

class FakeEnterpriseSearchAggregatorProvider
    : public EnterpriseSearchAggregatorProvider {
 public:
  using EnterpriseSearchAggregatorProvider::CreateMatch;
  using EnterpriseSearchAggregatorProvider ::done_;
  using EnterpriseSearchAggregatorProvider::EnterpriseSearchAggregatorProvider;
  using EnterpriseSearchAggregatorProvider::IsProviderAllowed;

 protected:
  ~FakeEnterpriseSearchAggregatorProvider() override = default;
};

class EnterpriseSearchAggregatorProviderTest : public testing::Test {
 protected:
  EnterpriseSearchAggregatorProviderTest() {
    client_ = std::make_unique<FakeAutocompleteProviderClient>();
    provider_ = new FakeEnterpriseSearchAggregatorProvider(client_.get());
  }
  void InitClient();
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<FakeEnterpriseSearchAggregatorProvider> provider_;
};

void EnterpriseSearchAggregatorProviderTest::InitClient() {
  EXPECT_CALL(*client_.get(), IsAuthenticated()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));
}

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

// Test that the provider runs only when allowed.
TEST_F(EnterpriseSearchAggregatorProviderTest, IsProviderAllowed) {
  InitClient();

  // Feature must be enabled.
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::SearchAggregatorProvider>
      scoped_config;
  scoped_config.Get().enabled = true;
  scoped_config.Get().name = "test";
  scoped_config.Get().shortcut = "test";
  scoped_config.Get().search_url = "example.com/{searchTerms}";
  scoped_config.Get().suggest_url = "example.com";

  AutocompleteInput ac_input = AutocompleteInput(
      u"text text", metrics::OmniboxEventProto::OTHER, TestSchemeClassifier());

  // Check IsProviderAllowed() returns true when all conditions pass.
  EXPECT_TRUE(provider_->IsProviderAllowed(ac_input));

  {
    // Should not be an incognito window.
    EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(true));
    EXPECT_FALSE(provider_->IsProviderAllowed(ac_input));
    EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));
    EXPECT_TRUE(provider_->IsProviderAllowed(ac_input));
  }

  {
    // Feature must be enabled.
    scoped_config.Get().enabled = false;
    EXPECT_FALSE(provider_->IsProviderAllowed(ac_input));
    scoped_config.Get().enabled = true;
    EXPECT_TRUE(provider_->IsProviderAllowed(ac_input));
  }
}

// Test that a call to ::Start will stop old requests to prevent their results
// from appearing with the new input.
TEST_F(EnterpriseSearchAggregatorProviderTest, StartCallsStop) {
  InitClient();

  AutocompleteInput invalid_input(u"@test", metrics::OmniboxEventProto::OTHER,
                                  TestSchemeClassifier());
  invalid_input.set_omit_asynchronous_matches(false);

  provider_->done_ = false;
  provider_->Start(invalid_input, false);
  EXPECT_TRUE(provider_->done());
}
