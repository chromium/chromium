// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engines_test_util.h"

#include <utility>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

std::unique_ptr<TemplateURLData> GenerateDummyTemplateURLData(
    const std::string& keyword) {
  auto data = std::make_unique<TemplateURLData>();
  data->SetShortName(base::UTF8ToUTF16(keyword + "name"));
  data->SetKeyword(base::UTF8ToUTF16(keyword));
  data->SetURL(std::string("http://") + keyword + "foo/{searchTerms}");
  data->suggestions_url = std::string("http://") + keyword + "sugg";
  data->alternate_urls.push_back(std::string("http://") + keyword + "foo/alt");
  data->favicon_url = GURL("http://icon1");
  data->safe_for_autoreplace = true;
  data->input_encodings = {"UTF-8", "UTF-16"};
  data->date_created = base::Time();
  data->last_modified = base::Time();
  return data;
}

void ExpectSimilar(const TemplateURLData* expected,
                   const TemplateURLData* actual) {
  ASSERT_TRUE(expected);
  ASSERT_TRUE(actual);

  EXPECT_EQ(expected->short_name(), actual->short_name());
  EXPECT_EQ(expected->keyword(), actual->keyword());
  EXPECT_EQ(expected->url(), actual->url());
  EXPECT_EQ(expected->suggestions_url, actual->suggestions_url);
  EXPECT_EQ(expected->image_url, actual->image_url);
  EXPECT_EQ(expected->new_tab_url, actual->new_tab_url);
  EXPECT_EQ(expected->contextual_search_url, actual->contextual_search_url);

  EXPECT_EQ(expected->search_url_post_params, actual->search_url_post_params);
  EXPECT_EQ(expected->suggestions_url_post_params,
            actual->suggestions_url_post_params);
  EXPECT_EQ(expected->image_url_post_params, actual->image_url_post_params);

  EXPECT_EQ(expected->favicon_url, actual->favicon_url);
  EXPECT_EQ(expected->safe_for_autoreplace, actual->safe_for_autoreplace);
  EXPECT_EQ(expected->input_encodings, actual->input_encodings);
  EXPECT_EQ(expected->alternate_urls, actual->alternate_urls);
  EXPECT_EQ(expected->created_from_play_api, actual->created_from_play_api);
}

void SetExtensionDefaultSearchInPrefs(
    sync_preferences::TestingPrefServiceSyncable* prefs,
    const TemplateURLData& data) {
  std::unique_ptr<base::DictionaryValue> entry =
      TemplateURLDataToDictionary(data);
  prefs->SetExtensionPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName,
      std::move(entry));
}

void RemoveExtensionDefaultSearchFromPrefs(
    sync_preferences::TestingPrefServiceSyncable* prefs) {
  prefs->RemoveExtensionPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);
}
