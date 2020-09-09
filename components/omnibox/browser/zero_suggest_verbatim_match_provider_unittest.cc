// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_verbatim_match_provider.h"

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

class ZeroSuggestVerbatimMatchProviderTest
    : public testing::TestWithParam<
          metrics::OmniboxEventProto::PageClassification> {
 public:
  ZeroSuggestVerbatimMatchProviderTest() = default;
  void SetUp() override;

 protected:
  bool IsVerbatimMatchEligible() const;
  scoped_refptr<ZeroSuggestVerbatimMatchProvider> provider_;
  MockAutocompleteProviderClient mock_client_;
};

bool ZeroSuggestVerbatimMatchProviderTest::IsVerbatimMatchEligible() const {
  switch (GetParam()) {
    case metrics::OmniboxEventProto::OTHER:
    case metrics::OmniboxEventProto::
        SEARCH_RESULT_PAGE_DOING_SEARCH_TERM_REPLACEMENT:
    case metrics::OmniboxEventProto::
        SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT:
      return true;
    default:
      return false;
  }
}

void ZeroSuggestVerbatimMatchProviderTest::SetUp() {
  provider_ = new ZeroSuggestVerbatimMatchProvider(&mock_client_);
  ON_CALL(mock_client_, IsOffTheRecord()).WillByDefault([] { return false; });
  ON_CALL(mock_client_, Classify)
      .WillByDefault(
          [](const base::string16& text, bool prefer_keyword,
             bool allow_exact_keyword_match,
             metrics::OmniboxEventProto::PageClassification page_classification,
             AutocompleteMatch* match,
             GURL* alternate_nav_url) { match->destination_url = GURL(text); });
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest,
       NoVerbatimMatchWithUserTextInOmnibox) {
  std::string query("user input");
  std::string url("https://google.com/search?q=test");
  AutocompleteInput input(base::ASCIIToUTF16(query), GetParam(),
                          TestSchemeClassifier());
  input.set_current_url(GURL(url));
  input.set_focus_type(OmniboxFocusType::DEFAULT);
  provider_->Start(input, false);

  // Clobber state should never generate a verbatim match.
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest,
       NoVerbatimMatchWithUserTextInOmniboxInIncognito) {
  std::string query("user input");
  std::string url("https://google.com/search?q=test");
  AutocompleteInput input(base::ASCIIToUTF16(query), GetParam(),
                          TestSchemeClassifier());
  input.set_current_url(GURL(url));
  input.set_focus_type(OmniboxFocusType::DEFAULT);
  ON_CALL(mock_client_, IsOffTheRecord()).WillByDefault([] { return true; });
  provider_->Start(input, false);

  // Clobber state should never generate a verbatim match.
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest, OffersVerbatimMatchOnFocus) {
  std::string url("https://www.wired.com/");
  AutocompleteInput input(base::ASCIIToUTF16(url), GetParam(),
                          TestSchemeClassifier());
  input.set_current_url(GURL(url));
  input.set_focus_type(OmniboxFocusType::ON_FOCUS);
  provider_->Start(input, false);
  ASSERT_EQ(IsVerbatimMatchEligible(), provider_->matches().size() > 0);
  // Note: we intentionally do not validate the match content here.
  // The content is populated either by HistoryURLProvider or
  // AutocompleteProviderClient both of which we would have to mock for this
  // test. As a result, the test would validate what the mocks fill in.
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest,
       OffersVerbatimMatchOnFocusInIncognito) {
  std::string url("https://www.wired.com/");
  AutocompleteInput input(base::ASCIIToUTF16(url), GetParam(),
                          TestSchemeClassifier());
  input.set_current_url(GURL(url));
  input.set_focus_type(OmniboxFocusType::ON_FOCUS);
  ON_CALL(mock_client_, IsOffTheRecord()).WillByDefault([] { return true; });
  provider_->Start(input, false);
  ASSERT_EQ(IsVerbatimMatchEligible(), provider_->matches().size() > 0);
  // Note: we intentionally do not validate the match content here.
  // The content is populated either by HistoryURLProvider or
  // AutocompleteProviderClient both of which we would have to mock for this
  // test. As a result, the test would validate what the mocks fill in.
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest, NoVerbatimMatchWithEmptyInput) {
  std::string url("https://www.wired.com/");
  AutocompleteInput input(base::string16(),  // Note: empty input.
                          GetParam(), TestSchemeClassifier());
  input.set_current_url(GURL(url));
  input.set_focus_type(OmniboxFocusType::DEFAULT);
  provider_->Start(input, false);
  ASSERT_TRUE(provider_->matches().empty());
  // Note: we intentionally do not validate the match content here.
  // The content is populated either by HistoryURLProvider or
  // AutocompleteProviderClient both of which we would have to mock for this
  // test. As a result, the test would validate what the mocks fill in.
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest,
       NoVerbatimMatchWithEmptyInputInIncognito) {
  std::string url("https://www.wired.com/");
  AutocompleteInput input(base::string16(),  // Note: empty input.
                          GetParam(), TestSchemeClassifier());
  input.set_current_url(GURL(url));
  input.set_focus_type(OmniboxFocusType::DEFAULT);
  ON_CALL(mock_client_, IsOffTheRecord()).WillByDefault([] { return true; });
  provider_->Start(input, false);
  ASSERT_TRUE(provider_->matches().empty());
  // Note: we intentionally do not validate the match content here.
  // The content is populated either by HistoryURLProvider or
  // AutocompleteProviderClient both of which we would have to mock for this
  // test. As a result, the test would validate what the mocks fill in.
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest, NoVerbatimMatchOnClearInput) {
  std::string url("https://www.wired.com/");
  AutocompleteInput input(base::string16(),  // Note: empty input.
                          GetParam(), TestSchemeClassifier());
  input.set_current_url(GURL(url));
  input.set_focus_type(OmniboxFocusType::DELETED_PERMANENT_TEXT);
  provider_->Start(input, false);
  ASSERT_TRUE(provider_->matches().empty());
  // Note: we intentionally do not validate the match content here.
  // The content is populated either by HistoryURLProvider or
  // AutocompleteProviderClient both of which we would have to mock for this
  // test. As a result, the test would validate what the mocks fill in.
}

TEST_P(ZeroSuggestVerbatimMatchProviderTest,
       NoVerbatimMatchOnClearInputInIncognito) {
  std::string url("https://www.wired.com/");
  AutocompleteInput input(base::string16(),  // Note: empty input.
                          GetParam(), TestSchemeClassifier());
  input.set_current_url(GURL(url));
  input.set_focus_type(OmniboxFocusType::DELETED_PERMANENT_TEXT);
  ON_CALL(mock_client_, IsOffTheRecord()).WillByDefault([] { return true; });
  provider_->Start(input, false);
  ASSERT_TRUE(provider_->matches().empty());
  // Note: we intentionally do not validate the match content here.
  // The content is populated either by HistoryURLProvider or
  // AutocompleteProviderClient both of which we would have to mock for this
  // test. As a result, the test would validate what the mocks fill in.
}

INSTANTIATE_TEST_SUITE_P(
    ZeroSuggestVerbatimMatchProviderNonIncognitoTests,
    ZeroSuggestVerbatimMatchProviderTest,
    ::testing::Values(
        // Variants that should offer verbatim match.
        metrics::OmniboxEventProto::OTHER,
        metrics::OmniboxEventProto::
            SEARCH_RESULT_PAGE_DOING_SEARCH_TERM_REPLACEMENT,
        metrics::OmniboxEventProto::
            SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,

        // Variants that should offer no verbatim match.
        metrics::OmniboxEventProto::NTP,
        metrics::OmniboxEventProto::BLANK,
        metrics::OmniboxEventProto::HOME_PAGE,
        metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS),

    // Ensure clarity when error message is printed out.
    +[](const ::testing::TestParamInfo<
         metrics::OmniboxEventProto::PageClassification> context)
        -> std::string {
      return metrics::OmniboxEventProto::PageClassification_Name(context.param);
    });
