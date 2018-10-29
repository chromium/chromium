// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_suggestions_fetch.h"

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_suggestions {

namespace {

TEST(ContextualSuggestionsFetch, GetFetchEndpoint_Default) {
  EXPECT_EQ("https://www.google.com/httpservice/web/ExploreService/GetPivots/",
            ContextualSuggestionsFetch::GetFetchEndpoint());
}

TEST(ContextualSuggestionsFetch, GetFetchEndpoint_CommandLine_MissingValue) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII("contextual-suggestions-fetch-endpoint", "");
  EXPECT_EQ("https://www.google.com/httpservice/web/ExploreService/GetPivots/",
            ContextualSuggestionsFetch::GetFetchEndpoint());
}

TEST(ContextualSuggestionsFetch, GetFetchEndpoint_CommandLine_NonHTTPS) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII("contextual-suggestions-fetch-endpoint",
                                  "http://test.com");
  EXPECT_EQ("https://www.google.com/httpservice/web/ExploreService/GetPivots/",
            ContextualSuggestionsFetch::GetFetchEndpoint());
}

TEST(ContextualSuggestionsFetch, GetFetchEndpoint_CommandLine_ProperEndpoint) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII("contextual-suggestions-fetch-endpoint",
                                  "https://test.com");
  EXPECT_EQ("https://test.com/httpservice/web/ExploreService/GetPivots/",
            ContextualSuggestionsFetch::GetFetchEndpoint());
}

TEST(ContextualSuggestionsFetch, GetFetchEndpoint_Feature_NoParameter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kContextualSuggestionsButton);
  EXPECT_EQ("https://www.google.com/httpservice/web/ExploreService/GetPivots/",
            ContextualSuggestionsFetch::GetFetchEndpoint());
}

TEST(ContextualSuggestionsFetch, GetFetchEndpoint_Feature_EmptyParameter) {
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> parameters;
  parameters["contextual-suggestions-fetch-endpoint"] = "";
  feature_list.InitAndEnableFeatureWithParameters(kContextualSuggestionsButton,
                                                  parameters);
  EXPECT_EQ("https://www.google.com/httpservice/web/ExploreService/GetPivots/",
            ContextualSuggestionsFetch::GetFetchEndpoint());
}

TEST(ContextualSuggestionsFetch, GetFetchEndpoint_Feature_NonHTTPS) {
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> parameters;
  parameters["contextual-suggestions-fetch-endpoint"] = "http://test.com";
  feature_list.InitAndEnableFeatureWithParameters(kContextualSuggestionsButton,
                                                  parameters);
  EXPECT_EQ("https://www.google.com/httpservice/web/ExploreService/GetPivots/",
            ContextualSuggestionsFetch::GetFetchEndpoint());
}

TEST(ContextualSuggestionsFetch, GetFetchEndpoint_Feature_WithParameter) {
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> parameters;
  parameters["contextual-suggestions-fetch-endpoint"] = "https://test.com";
  feature_list.InitAndEnableFeatureWithParameters(kContextualSuggestionsButton,
                                                  parameters);
  EXPECT_EQ("https://test.com/httpservice/web/ExploreService/GetPivots/",
            ContextualSuggestionsFetch::GetFetchEndpoint());
}

}  // namespace

}  // namespace contextual_suggestions
