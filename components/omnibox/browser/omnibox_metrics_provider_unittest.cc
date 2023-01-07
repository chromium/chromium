// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_metrics_provider.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/match_compare.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/window_open_disposition.h"

class OmniboxMetricsProviderTest : public testing::Test {
 public:
  OmniboxMetricsProviderTest() = default;
  ~OmniboxMetricsProviderTest() override = default;

  void SetUp() override {
    provider_ = std::make_unique<OmniboxMetricsProvider>();
  }

  void TearDown() override { provider_.reset(); }

  OmniboxLog BuildOmniboxLog(const AutocompleteResult& result,
                             size_t selected_index) {
    return OmniboxLog(
        u"my text", /*just_deleted_text=*/false, metrics::OmniboxInputType::URL,
        /*in_keyword_mode=*/false,
        metrics::OmniboxEventProto_KeywordModeEntryMethod_INVALID,
        /*is_popup_open=*/false, /*selected_index=*/selected_index,
        WindowOpenDisposition::CURRENT_TAB, /*is_paste_and_go=*/false,
        SessionID::NewUnique(),
        metrics::OmniboxEventProto::PageClassification::
            OmniboxEventProto_PageClassification_NTP_REALBOX,
        /*elapsed_time_since_user_first_modified_omnibox=*/base::TimeDelta(),
        /*completed_length=*/0,
        /*elapsed_time_since_last_change_to_default_match=*/base::TimeDelta(),
        result, GURL("https://www.example.com/"));
  }

  AutocompleteMatch BuildMatch(AutocompleteMatch::Type type) {
    return AutocompleteMatch(nullptr, 0, false, type);
  }

  void RecordLogAndVerify(const OmniboxLog& log,
                          int32_t sample,
                          int32_t expected_count) {
    base::HistogramTester histogram_tester;
    provider_->RecordOmniboxOpenedURLClientSummarizedResultType(log);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionUsed.ClientSummarizedResultType", sample,
        expected_count);
  }

 protected:
  std::unique_ptr<OmniboxMetricsProvider> provider_;
};

TEST_F(OmniboxMetricsProviderTest, ClientSummarizedResultTypeSingleURL) {
  AutocompleteResult result;
  result.AppendMatches(
      {BuildMatch(AutocompleteMatch::Type::URL_WHAT_YOU_TYPED)});
  OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/0);
  RecordLogAndVerify(log, /*sample=*/0, /*expected_count=*/1);
}

TEST_F(OmniboxMetricsProviderTest, ClientSummarizedResultTypeSingleSearch) {
  AutocompleteResult result;
  result.AppendMatches({BuildMatch(AutocompleteMatch::Type::SEARCH_SUGGEST)});
  OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/0);
  RecordLogAndVerify(log, /*sample=*/1, /*expected_count=*/1);
}

TEST_F(OmniboxMetricsProviderTest, ClientSummarizedResultTypeMultipleSearch) {
  AutocompleteResult result;
  result.AppendMatches(
      {BuildMatch(AutocompleteMatch::Type::URL_WHAT_YOU_TYPED),
       BuildMatch(AutocompleteMatch::Type::SEARCH_SUGGEST),
       BuildMatch(AutocompleteMatch::Type::URL_WHAT_YOU_TYPED)});
  OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/1);
  RecordLogAndVerify(log, /*sample=*/1, /*expected_count=*/1);
}
