// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_data_util.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/template_url_data.h"
#include "url/gurl.h"

std::unique_ptr<TemplateURLData> TemplateURLDataFromDictionary(
    const base::DictionaryValue& dict) {
  std::string search_url;
  base::string16 keyword;
  base::string16 short_name;
  dict.GetString(DefaultSearchManager::kURL, &search_url);
  dict.GetString(DefaultSearchManager::kKeyword, &keyword);
  dict.GetString(DefaultSearchManager::kShortName, &short_name);
  // Check required TemplateURLData fields first.
  if (search_url.empty() || keyword.empty() || short_name.empty())
    return std::unique_ptr<TemplateURLData>();

  auto result = std::make_unique<TemplateURLData>();
  result->SetKeyword(keyword);
  result->SetURL(search_url);

  std::string id;
  dict.GetString(DefaultSearchManager::kID, &id);
  base::StringToInt64(id, &result->id);

  result->SetShortName(short_name);
  dict.GetInteger(DefaultSearchManager::kPrepopulateID,
                  &result->prepopulate_id);
  dict.GetString(DefaultSearchManager::kSyncGUID, &result->sync_guid);
  dict.GetString(DefaultSearchManager::kSuggestionsURL,
                 &result->suggestions_url);

  dict.GetString(DefaultSearchManager::kImageURL, &result->image_url);
  dict.GetString(DefaultSearchManager::kNewTabURL, &result->new_tab_url);
  dict.GetString(DefaultSearchManager::kContextualSearchURL,
                 &result->contextual_search_url);
  std::string favicon_url;
  std::string originating_url;
  std::string logo_url;
  std::string doodle_url;
  dict.GetString(DefaultSearchManager::kFaviconURL, &favicon_url);
  dict.GetString(DefaultSearchManager::kOriginatingURL, &originating_url);
  dict.GetString(DefaultSearchManager::kLogoURL, &logo_url);
  dict.GetString(DefaultSearchManager::kDoodleURL, &doodle_url);
  result->favicon_url = GURL(favicon_url);
  result->originating_url = GURL(originating_url);
  result->logo_url = GURL(logo_url);
  result->doodle_url = GURL(doodle_url);

  dict.GetString(DefaultSearchManager::kSearchURLPostParams,
                 &result->search_url_post_params);
  dict.GetString(DefaultSearchManager::kSuggestionsURLPostParams,
                 &result->suggestions_url_post_params);
  dict.GetString(DefaultSearchManager::kImageURLPostParams,
                 &result->image_url_post_params);
  dict.GetBoolean(DefaultSearchManager::kSafeForAutoReplace,
                  &result->safe_for_autoreplace);

  std::string date_created_str;
  std::string last_modified_str;
  std::string last_visited_str;
  dict.GetString(DefaultSearchManager::kDateCreated, &date_created_str);
  dict.GetString(DefaultSearchManager::kLastModified, &last_modified_str);
  dict.GetString(DefaultSearchManager::kLastVisited, &last_visited_str);

  int64_t date_created = 0;
  if (base::StringToInt64(date_created_str, &date_created))
    result->date_created = base::Time::FromInternalValue(date_created);

  int64_t last_modified = 0;
  if (base::StringToInt64(last_modified_str, &last_modified))
    result->last_modified = base::Time::FromInternalValue(last_modified);

  int64_t last_visited = 0;
  if (base::StringToInt64(last_visited_str, &last_visited))
    result->last_visited = base::Time::FromInternalValue(last_visited);

  dict.GetInteger(DefaultSearchManager::kUsageCount, &result->usage_count);

  const base::ListValue* alternate_urls = nullptr;
  if (dict.GetList(DefaultSearchManager::kAlternateURLs, &alternate_urls)) {
    for (const auto& it : *alternate_urls) {
      std::string alternate_url;
      if (it.GetAsString(&alternate_url))
        result->alternate_urls.push_back(std::move(alternate_url));
    }
  }

  const base::ListValue* encodings = nullptr;
  if (dict.GetList(DefaultSearchManager::kInputEncodings, &encodings)) {
    for (const auto& it : *encodings) {
      std::string encoding;
      if (it.GetAsString(&encoding))
        result->input_encodings.push_back(std::move(encoding));
    }
  }

  dict.GetBoolean(DefaultSearchManager::kCreatedByPolicy,
                  &result->created_by_policy);
  dict.GetBoolean(DefaultSearchManager::kCreatedFromPlayAPI,
                  &result->created_from_play_api);
  return result;
}

std::unique_ptr<base::DictionaryValue> TemplateURLDataToDictionary(
    const TemplateURLData& data) {
  auto url_dict = std::make_unique<base::DictionaryValue>();
  url_dict->SetString(DefaultSearchManager::kID, base::NumberToString(data.id));
  url_dict->SetString(DefaultSearchManager::kShortName, data.short_name());
  url_dict->SetString(DefaultSearchManager::kKeyword, data.keyword());
  url_dict->SetInteger(DefaultSearchManager::kPrepopulateID,
                       data.prepopulate_id);
  url_dict->SetString(DefaultSearchManager::kSyncGUID, data.sync_guid);

  url_dict->SetString(DefaultSearchManager::kURL, data.url());
  url_dict->SetString(DefaultSearchManager::kSuggestionsURL,
                      data.suggestions_url);
  url_dict->SetString(DefaultSearchManager::kImageURL, data.image_url);
  url_dict->SetString(DefaultSearchManager::kNewTabURL, data.new_tab_url);
  url_dict->SetString(DefaultSearchManager::kContextualSearchURL,
                      data.contextual_search_url);
  url_dict->SetString(DefaultSearchManager::kFaviconURL,
                      data.favicon_url.spec());
  url_dict->SetString(DefaultSearchManager::kOriginatingURL,
                      data.originating_url.spec());
  url_dict->SetString(DefaultSearchManager::kLogoURL, data.logo_url.spec());
  url_dict->SetString(DefaultSearchManager::kDoodleURL, data.doodle_url.spec());

  url_dict->SetString(DefaultSearchManager::kSearchURLPostParams,
                      data.search_url_post_params);
  url_dict->SetString(DefaultSearchManager::kSuggestionsURLPostParams,
                      data.suggestions_url_post_params);
  url_dict->SetString(DefaultSearchManager::kImageURLPostParams,
                      data.image_url_post_params);

  url_dict->SetBoolean(DefaultSearchManager::kSafeForAutoReplace,
                       data.safe_for_autoreplace);

  url_dict->SetString(
      DefaultSearchManager::kDateCreated,
      base::NumberToString(data.date_created.ToInternalValue()));
  url_dict->SetString(
      DefaultSearchManager::kLastModified,
      base::NumberToString(data.last_modified.ToInternalValue()));
  url_dict->SetString(
      DefaultSearchManager::kLastVisited,
      base::NumberToString(data.last_visited.ToInternalValue()));
  url_dict->SetInteger(DefaultSearchManager::kUsageCount, data.usage_count);

  auto alternate_urls = std::make_unique<base::ListValue>();
  for (const auto& alternate_url : data.alternate_urls)
    alternate_urls->AppendString(alternate_url);

  url_dict->Set(DefaultSearchManager::kAlternateURLs,
                std::move(alternate_urls));

  auto encodings = std::make_unique<base::ListValue>();
  for (const auto& input_encoding : data.input_encodings)
    encodings->AppendString(input_encoding);
  url_dict->Set(DefaultSearchManager::kInputEncodings, std::move(encodings));

  url_dict->SetBoolean(DefaultSearchManager::kCreatedByPolicy,
                       data.created_by_policy);
  url_dict->SetBoolean(DefaultSearchManager::kCreatedFromPlayAPI,
                       data.created_from_play_api);
  return url_dict;
}

std::unique_ptr<TemplateURLData> TemplateURLDataFromPrepopulatedEngine(
    const TemplateURLPrepopulateData::PrepopulatedEngine& engine) {
  base::ListValue alternate_urls;
  if (engine.alternate_urls) {
    for (size_t i = 0; i < engine.alternate_urls_size; ++i)
      alternate_urls.AppendString(std::string(engine.alternate_urls[i]));
  }

  return std::make_unique<TemplateURLData>(
      base::WideToUTF16(engine.name), base::WideToUTF16(engine.keyword),
      engine.search_url, engine.suggest_url, engine.image_url,
      engine.new_tab_url, engine.contextual_search_url, engine.logo_url,
      engine.doodle_url, engine.search_url_post_params,
      engine.suggest_url_post_params, engine.image_url_post_params,
      engine.favicon_url, engine.encoding, alternate_urls, engine.id);
}

std::unique_ptr<TemplateURLData> TemplateURLDataFromOverrideDictionary(
    const base::DictionaryValue& engine) {
  base::string16 name;
  base::string16 keyword;
  std::string search_url;
  std::string favicon_url;
  std::string encoding;
  int id = -1;
  // The following fields are required for each search engine configuration.
  if (engine.GetString("name", &name) && !name.empty() &&
      engine.GetString("keyword", &keyword) && !keyword.empty() &&
      engine.GetString("search_url", &search_url) && !search_url.empty() &&
      engine.GetString("favicon_url", &favicon_url) && !favicon_url.empty() &&
      engine.GetString("encoding", &encoding) && !encoding.empty() &&
      engine.GetInteger("id", &id)) {
    // These fields are optional.
    std::string suggest_url;
    std::string image_url;
    std::string new_tab_url;
    std::string contextual_search_url;
    std::string logo_url;
    std::string doodle_url;
    std::string search_url_post_params;
    std::string suggest_url_post_params;
    std::string image_url_post_params;
    base::ListValue empty_list;
    const base::ListValue* alternate_urls = &empty_list;
    engine.GetString("suggest_url", &suggest_url);
    engine.GetString("image_url", &image_url);
    engine.GetString("new_tab_url", &new_tab_url);
    engine.GetString("contextual_search_url", &contextual_search_url);
    engine.GetString("logo_url", &logo_url);
    engine.GetString("doodle_url", &doodle_url);
    engine.GetString("search_url_post_params", &search_url_post_params);
    engine.GetString("suggest_url_post_params", &suggest_url_post_params);
    engine.GetString("image_url_post_params", &image_url_post_params);
    engine.GetList("alternate_urls", &alternate_urls);
    return std::make_unique<TemplateURLData>(
        name, keyword, search_url, suggest_url, image_url, new_tab_url,
        contextual_search_url, logo_url, doodle_url, search_url_post_params,
        suggest_url_post_params, image_url_post_params, favicon_url, encoding,
        *alternate_urls, id);
  }
  return std::unique_ptr<TemplateURLData>();
}
