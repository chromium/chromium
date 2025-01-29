// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "enterprise_search_aggregator_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/search_engines/template_url_data.h"
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
  using EnterpriseSearchAggregatorProvider::matches_;

 protected:
  ~FakeEnterpriseSearchAggregatorProvider() override = default;
};

class EnterpriseSearchAggregatorProviderTestBase {
 protected:
  EnterpriseSearchAggregatorProviderTestBase() {
    client_ = std::make_unique<FakeAutocompleteProviderClient>();
    provider_ = new FakeEnterpriseSearchAggregatorProvider(client_.get());
  }

  void InitClient() {
    EXPECT_CALL(*client_.get(), IsAuthenticated()).WillRepeatedly(Return(true));
    EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));
  }

  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::SearchAggregatorProvider>
  InitFeature() {
    omnibox_feature_configs::ScopedConfigForTesting<
        omnibox_feature_configs::SearchAggregatorProvider>
        scoped_config;
    scoped_config.Get().enabled = true;
    scoped_config.Get().name = "test";
    scoped_config.Get().shortcut = "test";
    scoped_config.Get().search_url = "example.com/{searchTerms}";
    scoped_config.Get().suggest_url = "example.com";
    return scoped_config;
  }

  std::vector<std::u16string> GetMatches() {
    std::vector<std::u16string> match_strings;
    for (const auto& m : provider_->matches_)
      match_strings.push_back(m.fill_into_edit);
    return match_strings;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<FakeEnterpriseSearchAggregatorProvider> provider_;
};

class EnterpriseSearchAggregatorProviderTest
    : public EnterpriseSearchAggregatorProviderTestBase,
      public testing::Test {};

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
  auto scoped_config = InitFeature();

  AutocompleteInput input(u"text text", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());

  // Check` IsProviderAllowed()` returns true when all conditions pass.
  EXPECT_TRUE(provider_->IsProviderAllowed(input));

  {
    // Should not be an incognito window.
    EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(true));
    EXPECT_FALSE(provider_->IsProviderAllowed(input));
    EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));
    EXPECT_TRUE(provider_->IsProviderAllowed(input));
  }

  {
    // Feature must be enabled.
    scoped_config.Get().enabled = false;
    EXPECT_FALSE(provider_->IsProviderAllowed(input));
    scoped_config.Get().enabled = true;
    EXPECT_TRUE(provider_->IsProviderAllowed(input));
  }
}

// Test that a call to `Start()` will stop old requests to prevent their results
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

class EnterpriseSearchAggregatorProviderFeaturedByPolicyTest
    : public EnterpriseSearchAggregatorProviderTestBase,
      public testing::TestWithParam<bool> {};

INSTANTIATE_TEST_SUITE_P(,
                         EnterpriseSearchAggregatorProviderFeaturedByPolicyTest,
                         testing::Bool());

TEST_P(EnterpriseSearchAggregatorProviderFeaturedByPolicyTest, CacheMatches) {
  // Setup.
  InitClient();
  TemplateURLData turl_data;
  turl_data.SetShortName(u"keyword");
  turl_data.SetKeyword(u"keyword");
  turl_data.SetURL("http://www.keyword.com/{searchTerms}");
  turl_data.is_active = TemplateURLData::ActiveStatus::kTrue;
  turl_data.featured_by_policy = GetParam();
  turl_data.policy_origin = TemplateURLData::PolicyOrigin::kSearchAggregator;
  client_->GetTemplateURLService()->Add(
      std::make_unique<TemplateURL>(turl_data));
  auto scoped_config = InitFeature();
  AutocompleteInput input(u"keyword query", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());

  // Call `Start()` to put debouncer on cooldown.
  provider_->Start(input, false);
  EXPECT_THAT(GetMatches(), testing::ElementsAre(u"https://wikipedia.org"));

  // Set an old match.
  provider_->matches_ = {provider_->CreateMatch(input, u"keyword", true, 1500,
                                                "https://cached.org", u"cached",
                                                u"cached")};

  // Call `Start()`, old match should still be present.
  provider_->Start(input, false);
  EXPECT_THAT(GetMatches(), testing::ElementsAre(u"https://cached.org"));

  // Wait for debouncer and request to complete, old match should be
  // replaced by new match.
  task_environment_.FastForwardBy(base::Minutes(1));

  EXPECT_THAT(GetMatches(), testing::ElementsAre(u"https://wikipedia.org"));
}
