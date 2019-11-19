// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<TemplateURLData> CreatePrepopulateTemplateURLData(
    int prepopulate_id,
    const std::string& keyword) {
  return std::make_unique<TemplateURLData>(
      base::ASCIIToUTF16("Search engine name"), base::ASCIIToUTF16(keyword),
      "https://search.url", nullptr /* suggest_url */, nullptr /* image_url */,
      nullptr /* new_tab_url */, nullptr /* contextual_search_url */,
      nullptr /* logo_url */, nullptr /* doodle_url */,
      nullptr /* search_url_post_params */,
      nullptr /* suggest_url_post_params */,
      nullptr /* image_url_post_params */, nullptr /* favicon_url */, "UTF-8",
      base::ListValue() /* alternate_urls_list */, prepopulate_id);
}

// Creates a TemplateURL with default values except for the prepopulate ID,
// keyword and TemplateURLID. Only use this in tests if your tests do not
// care about other fields.
std::unique_ptr<TemplateURL> CreatePrepopulateTemplateURL(
    int prepopulate_id,
    const std::string& keyword,
    TemplateURLID id,
    bool is_play_api_turl = false) {
  std::unique_ptr<TemplateURLData> data =
      CreatePrepopulateTemplateURLData(prepopulate_id, keyword);
  data->id = id;
  data->created_from_play_api = is_play_api_turl;
  return std::make_unique<TemplateURL>(*data);
}

}  // namespace

TEST(TemplateURLServiceUtilTest, RemoveDuplicatePrepopulateIDs) {
  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_turls;
  TemplateURLService::OwnedTemplateURLVector local_turls;

  prepopulated_turls.push_back(CreatePrepopulateTemplateURLData(1, "winner4"));
  prepopulated_turls.push_back(CreatePrepopulateTemplateURLData(2, "xxx"));
  prepopulated_turls.push_back(CreatePrepopulateTemplateURLData(3, "yyy"));

  // Create a sets of different TURLs grouped by prepopulate ID. Each group
  // will test a different heuristic of RemoveDuplicatePrepopulateIDs.
  // Ignored set - These should be left alone as they do not have valid
  // prepopulate IDs.
  local_turls.push_back(CreatePrepopulateTemplateURL(0, "winner1", 4));
  local_turls.push_back(CreatePrepopulateTemplateURL(0, "winner2", 5));
  local_turls.push_back(CreatePrepopulateTemplateURL(0, "winner3", 6));
  size_t num_non_prepopulated_urls = local_turls.size();

  // Keyword match set - Prefer the one that matches the keyword of the
  // prepopulate ID.
  local_turls.push_back(CreatePrepopulateTemplateURL(1, "loser1", 7));
  local_turls.push_back(CreatePrepopulateTemplateURL(1, "loser2", 8));
  local_turls.push_back(CreatePrepopulateTemplateURL(1, "winner4", 9));

  // Default set - Prefer the default search engine over all other criteria.
  // The last one is the default. It will be passed as the
  // default_search_provider parameter to RemoveDuplicatePrepopulateIDs.
  local_turls.push_back(CreatePrepopulateTemplateURL(2, "loser3", 10));
  local_turls.push_back(CreatePrepopulateTemplateURL(2, "xxx", 11));
  local_turls.push_back(CreatePrepopulateTemplateURL(2, "winner5", 12));
  TemplateURL* default_turl = local_turls.back().get();

  // ID set - Prefer the lowest TemplateURLID if the keywords don't match and if
  // none are the default.
  local_turls.push_back(CreatePrepopulateTemplateURL(3, "winner6", 13));
  local_turls.push_back(CreatePrepopulateTemplateURL(3, "loser5", 14));
  local_turls.push_back(CreatePrepopulateTemplateURL(3, "loser6", 15));

  RemoveDuplicatePrepopulateIDs(nullptr, prepopulated_turls, default_turl,
                                &local_turls, SearchTermsData(), nullptr);

  // Verify that the expected local TURLs survived the process.
  EXPECT_EQ(local_turls.size(),
            prepopulated_turls.size() + num_non_prepopulated_urls);
  for (const auto& turl : local_turls) {
    EXPECT_TRUE(base::StartsWith(turl->keyword(), base::ASCIIToUTF16("winner"),
                                 base::CompareCase::SENSITIVE));
  }
}

// Tests correct interaction of Play API search engine during prepopulated list
// update.
TEST(TemplateURLServiceUtilTest, MergeEnginesFromPrepopulateData_PlayAPI) {
  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_turls;
  TemplateURLService::OwnedTemplateURLVector local_turls;

  // Start with single search engine created from Play API data.
  local_turls.push_back(CreatePrepopulateTemplateURL(0, "play", 1, true));

  // Test that prepopulated search engine with matching keyword is merged with
  // Play API search engine. Search URL should come from Play API search engine.
  const std::string prepopulated_search_url = "http://prepopulated.url";
  prepopulated_turls.push_back(CreatePrepopulateTemplateURLData(1, "play"));
  prepopulated_turls.back()->SetURL(prepopulated_search_url);
  MergeEnginesFromPrepopulateData(nullptr, &prepopulated_turls, &local_turls,
                                  nullptr, nullptr);
  ASSERT_EQ(local_turls.size(), 1U);
  // Merged search engine should have both Play API flag and valid
  // prepopulate_id.
  EXPECT_TRUE(local_turls[0]->created_from_play_api());
  EXPECT_EQ(1, local_turls[0]->prepopulate_id());
  EXPECT_NE(prepopulated_search_url, local_turls[0]->url());

  // Test that merging prepopulated search engine with matching prepopulate_id
  // preserves keyword of Play API search engine.
  prepopulated_turls.clear();
  prepopulated_turls.push_back(CreatePrepopulateTemplateURLData(1, "play2"));
  MergeEnginesFromPrepopulateData(nullptr, &prepopulated_turls, &local_turls,
                                  nullptr, nullptr);
  ASSERT_EQ(local_turls.size(), 1U);
  EXPECT_TRUE(local_turls[0]->created_from_play_api());
  EXPECT_EQ(local_turls[0]->keyword(), base::ASCIIToUTF16("play"));

  // Test that removing search engine from prepopulated list doesn't delete Play
  // API search engine record.
  prepopulated_turls.clear();
  MergeEnginesFromPrepopulateData(nullptr, &prepopulated_turls, &local_turls,
                                  nullptr, nullptr);
  ASSERT_EQ(local_turls.size(), 1U);
  EXPECT_TRUE(local_turls[0]->created_from_play_api());
  EXPECT_EQ(local_turls[0]->prepopulate_id(), 0);
}
