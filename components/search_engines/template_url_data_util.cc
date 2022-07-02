// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_data_util.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

// Converts the C-style string `str` to a base::StringPiece making sure to avoid
// dereferencing nullptrs.
base::StringPiece ToStringPiece(const char* str) {
  return str ? base::StringPiece(str) : base::StringPiece();
}

}  // namespace

std::unique_ptr<TemplateURLData> TemplateURLDataFromDictionary(
    const base::Value& dict) {
  const std::string* search_url =
      dict.FindStringKey(DefaultSearchManager::kURL);
  const std::string* keyword =
      dict.FindStringKey(DefaultSearchManager::kKeyword);
  const std::string* short_name =
      dict.FindStringKey(DefaultSearchManager::kShortName);
  // Check required TemplateURLData fields first.
  if (!search_url || !keyword || !short_name) {
    return nullptr;
  }

  auto result = std::make_unique<TemplateURLData>();
  result->SetKeyword(base::UTF8ToUTF16(*keyword));
  result->SetURL(*search_url);

  const std::string* id = dict.FindStringKey(DefaultSearchManager::kID);
  if (id) {
    base::StringToInt64(*id, &result->id);
  }

  const std::string* string_value = nullptr;

  result->SetShortName(base::UTF8ToUTF16(*short_name));
  result->prepopulate_id = dict.FindIntKey(DefaultSearchManager::kPrepopulateID)
                               .value_or(result->prepopulate_id);
  result->starter_pack_id =
      dict.FindIntKey(DefaultSearchManager::kStarterPackId)
          .value_or(result->starter_pack_id);
  string_value = dict.FindStringKey(DefaultSearchManager::kSyncGUID);
  if (string_value) {
    result->sync_guid = *string_value;
  }
  string_value = dict.FindStringKey(DefaultSearchManager::kSuggestionsURL);
  if (string_value) {
    result->suggestions_url = *string_value;
  }

  string_value = dict.FindStringKey(DefaultSearchManager::kImageURL);
  if (string_value) {
    result->image_url = *string_value;
  }
  string_value = dict.FindStringKey(DefaultSearchManager::kNewTabURL);
  if (string_value) {
    result->new_tab_url = *string_value;
  }
  string_value = dict.FindStringKey(DefaultSearchManager::kContextualSearchURL);
  if (string_value) {
    result->contextual_search_url = *string_value;
  }

  string_value = dict.FindStringKey(DefaultSearchManager::kFaviconURL);
  if (string_value) {
    result->favicon_url = GURL(*string_value);
  }
  string_value = dict.FindStringKey(DefaultSearchManager::kOriginatingURL);
  if (string_value) {
    result->originating_url = GURL(*string_value);
  }
  string_value = dict.FindStringKey(DefaultSearchManager::kLogoURL);
  if (string_value) {
    result->logo_url = GURL(*string_value);
  }
  string_value = dict.FindStringKey(DefaultSearchManager::kDoodleURL);
  if (string_value) {
    result->doodle_url = GURL(*string_value);
  }

  const std::string* search_url_post_params =
      dict.FindStringKey(DefaultSearchManager::kSearchURLPostParams);
  if (search_url_post_params) {
    result->search_url_post_params = *search_url_post_params;
  }
  const std::string* suggestions_url_post_params =
      dict.FindStringKey(DefaultSearchManager::kSuggestionsURLPostParams);
  if (suggestions_url_post_params) {
    result->suggestions_url_post_params = *suggestions_url_post_params;
  }
  const std::string* image_url_post_params =
      dict.FindStringKey(DefaultSearchManager::kImageURLPostParams);
  if (image_url_post_params) {
    result->image_url_post_params = *image_url_post_params;
  }
  const std::string* side_search_param =
      dict.GetDict().FindString(DefaultSearchManager::kSideSearchParam);
  if (side_search_param) {
    result->side_search_param = *side_search_param;
  }
  absl::optional<bool> safe_for_autoreplace =
      dict.FindBoolKey(DefaultSearchManager::kSafeForAutoReplace);
  if (safe_for_autoreplace) {
    result->safe_for_autoreplace = *safe_for_autoreplace;
  }

  std::string date_created_str;
  std::string last_modified_str;
  std::string last_visited_str;

  string_value = dict.FindStringKey(DefaultSearchManager::kDateCreated);
  if (string_value) {
    date_created_str = *string_value;
  }
  string_value = dict.FindStringKey(DefaultSearchManager::kLastModified);
  if (string_value) {
    last_modified_str = *string_value;
  }
  string_value = dict.FindStringKey(DefaultSearchManager::kLastVisited);
  if (string_value) {
    last_visited_str = *string_value;
  }

  int64_t date_created = 0;
  if (base::StringToInt64(date_created_str, &date_created))
    result->date_created = base::Time::FromInternalValue(date_created);

  int64_t last_modified = 0;
  if (base::StringToInt64(last_modified_str, &last_modified))
    result->last_modified = base::Time::FromInternalValue(last_modified);

  int64_t last_visited = 0;
  if (base::StringToInt64(last_visited_str, &last_visited))
    result->last_visited = base::Time::FromInternalValue(last_visited);

  result->usage_count = dict.FindIntKey(DefaultSearchManager::kUsageCount)
                            .value_or(result->usage_count);

  const base::Value* alternate_urls =
      dict.FindListKey(DefaultSearchManager::kAlternateURLs);
  if (alternate_urls) {
    for (const auto& it : alternate_urls->GetListDeprecated()) {
      if (it.is_string())
        result->alternate_urls.push_back(it.GetString());
    }
  }

  const base::Value* encodings =
      dict.FindListKey(DefaultSearchManager::kInputEncodings);
  if (encodings) {
    for (const auto& it : encodings->GetListDeprecated()) {
      std::string encoding;
      if (it.is_string())
        result->input_encodings.push_back(it.GetString());
    }
  }

  result->created_by_policy =
      dict.FindBoolKey(DefaultSearchManager::kCreatedByPolicy)
          .value_or(result->created_by_policy);
  result->created_from_play_api =
      dict.FindBoolKey(DefaultSearchManager::kCreatedFromPlayAPI)
          .value_or(result->created_from_play_api);
  result->preconnect_to_search_url =
      dict.FindBoolKey(DefaultSearchManager::kPreconnectToSearchUrl)
          .value_or(result->preconnect_to_search_url);
  result->prefetch_likely_navigations =
      dict.FindBoolKey(DefaultSearchManager::kPrefetchLikelyNavigations)
          .value_or(result->prefetch_likely_navigations);
  result->is_active = static_cast<TemplateURLData::ActiveStatus>(
      dict.FindIntKey(DefaultSearchManager::kIsActive)
          .value_or(static_cast<int>(result->is_active)));
  return result;
}

std::unique_ptr<base::DictionaryValue> TemplateURLDataToDictionary(
    const TemplateURLData& data) {
  auto url_dict = std::make_unique<base::DictionaryValue>();
  url_dict->SetStringKey(DefaultSearchManager::kID,
                         base::NumberToString(data.id));
  url_dict->SetStringKey(DefaultSearchManager::kShortName, data.short_name());
  url_dict->SetStringKey(DefaultSearchManager::kKeyword, data.keyword());
  url_dict->SetIntKey(DefaultSearchManager::kPrepopulateID,
                      data.prepopulate_id);
  url_dict->SetIntKey(DefaultSearchManager::kStarterPackId,
                      data.starter_pack_id);
  url_dict->SetStringKey(DefaultSearchManager::kSyncGUID, data.sync_guid);

  url_dict->SetStringKey(DefaultSearchManager::kURL, data.url());
  url_dict->SetStringKey(DefaultSearchManager::kSuggestionsURL,
                         data.suggestions_url);
  url_dict->SetStringKey(DefaultSearchManager::kImageURL, data.image_url);
  url_dict->SetStringKey(DefaultSearchManager::kNewTabURL, data.new_tab_url);
  url_dict->SetStringKey(DefaultSearchManager::kContextualSearchURL,
                         data.contextual_search_url);
  url_dict->SetStringKey(DefaultSearchManager::kFaviconURL,
                         data.favicon_url.spec());
  url_dict->SetStringKey(DefaultSearchManager::kOriginatingURL,
                         data.originating_url.spec());
  url_dict->SetStringKey(DefaultSearchManager::kLogoURL, data.logo_url.spec());
  url_dict->SetStringKey(DefaultSearchManager::kDoodleURL,
                         data.doodle_url.spec());

  url_dict->SetStringKey(DefaultSearchManager::kSearchURLPostParams,
                         data.search_url_post_params);
  url_dict->SetStringKey(DefaultSearchManager::kSuggestionsURLPostParams,
                         data.suggestions_url_post_params);
  url_dict->SetStringKey(DefaultSearchManager::kImageURLPostParams,
                         data.image_url_post_params);
  url_dict->SetStringKey(DefaultSearchManager::kSideSearchParam,
                         data.side_search_param);

  url_dict->SetBoolKey(DefaultSearchManager::kSafeForAutoReplace,
                       data.safe_for_autoreplace);

  url_dict->SetStringKey(
      DefaultSearchManager::kDateCreated,
      base::NumberToString(data.date_created.ToInternalValue()));
  url_dict->SetStringKey(
      DefaultSearchManager::kLastModified,
      base::NumberToString(data.last_modified.ToInternalValue()));
  url_dict->SetStringKey(
      DefaultSearchManager::kLastVisited,
      base::NumberToString(data.last_visited.ToInternalValue()));
  url_dict->SetIntKey(DefaultSearchManager::kUsageCount, data.usage_count);

  base::ListValue alternate_urls;
  for (const auto& alternate_url : data.alternate_urls)
    alternate_urls.Append(alternate_url);

  url_dict->SetKey(DefaultSearchManager::kAlternateURLs,
                   std::move(alternate_urls));

  base::ListValue encodings;
  for (const auto& input_encoding : data.input_encodings)
    encodings.Append(input_encoding);
  url_dict->SetKey(DefaultSearchManager::kInputEncodings, std::move(encodings));

  url_dict->SetBoolKey(DefaultSearchManager::kCreatedByPolicy,
                       data.created_by_policy);
  url_dict->SetBoolKey(DefaultSearchManager::kCreatedFromPlayAPI,
                       data.created_from_play_api);
  url_dict->SetBoolKey(DefaultSearchManager::kPreconnectToSearchUrl,
                       data.preconnect_to_search_url);
  url_dict->SetBoolKey(DefaultSearchManager::kPrefetchLikelyNavigations,
                       data.prefetch_likely_navigations);
  url_dict->SetIntKey(DefaultSearchManager::kIsActive,
                      static_cast<int>(data.is_active));
  return url_dict;
}

std::unique_ptr<TemplateURLData> TemplateURLDataFromPrepopulatedEngine(
    const TemplateURLPrepopulateData::PrepopulatedEngine& engine) {
  base::ListValue alternate_urls;
  if (engine.alternate_urls) {
    for (size_t i = 0; i < engine.alternate_urls_size; ++i)
      alternate_urls.Append(std::string(engine.alternate_urls[i]));
  }

  return std::make_unique<TemplateURLData>(
      base::WideToUTF16(engine.name), base::WideToUTF16(engine.keyword),
      ToStringPiece(engine.search_url), ToStringPiece(engine.suggest_url),
      ToStringPiece(engine.image_url), ToStringPiece(engine.new_tab_url),
      ToStringPiece(engine.contextual_search_url),
      ToStringPiece(engine.logo_url), ToStringPiece(engine.doodle_url),
      ToStringPiece(engine.search_url_post_params),
      ToStringPiece(engine.suggest_url_post_params),
      ToStringPiece(engine.image_url_post_params),
      ToStringPiece(engine.side_search_param),
      ToStringPiece(engine.favicon_url), ToStringPiece(engine.encoding),
      alternate_urls,
      ToStringPiece(engine.preconnect_to_search_url) == "ALLOWED",
      ToStringPiece(engine.prefetch_likely_navigations) == "ALLOWED",
      engine.id);
}

std::unique_ptr<TemplateURLData> TemplateURLDataFromOverrideDictionary(
    const base::Value& engine) {
  const std::string* string_value = nullptr;

  std::u16string name;
  std::u16string keyword;
  std::string search_url;
  std::string favicon_url;
  std::string encoding;

  string_value = engine.FindStringKey("name");
  if (string_value) {
    name = base::UTF8ToUTF16(*string_value);
  }
  string_value = engine.FindStringKey("keyword");
  if (string_value) {
    keyword = base::UTF8ToUTF16(*string_value);
  }
  string_value = engine.FindStringKey("search_url");
  if (string_value) {
    search_url = *string_value;
  }
  string_value = engine.FindStringKey("favicon_url");
  if (string_value) {
    favicon_url = *string_value;
  }
  string_value = engine.FindStringKey("encoding");
  if (string_value) {
    encoding = *string_value;
  }
  absl::optional<int> id = engine.FindIntKey("id");

  // The following fields are required for each search engine configuration.
  if (!name.empty() && !keyword.empty() && !search_url.empty() &&
      !favicon_url.empty() && !encoding.empty() && id.has_value()) {
    // These fields are optional.
    base::Value empty_list;
    const base::Value* alternate_urls = engine.FindListKey("alternate_urls");
    if (!alternate_urls)
      alternate_urls = &empty_list;

    std::string suggest_url;
    std::string image_url;
    std::string new_tab_url;
    std::string contextual_search_url;
    std::string logo_url;
    std::string doodle_url;
    std::string search_url_post_params;
    std::string suggest_url_post_params;
    std::string image_url_post_params;
    std::string side_search_param;
    std::string preconnect_to_search_url;
    std::string prefetch_likely_navigations;

    string_value = engine.FindStringKey("suggest_url");
    if (string_value) {
      suggest_url = *string_value;
    }
    string_value = engine.FindStringKey("image_url");
    if (string_value) {
      image_url = *string_value;
    }
    string_value = engine.FindStringKey("new_tab_url");
    if (string_value) {
      new_tab_url = *string_value;
    }
    string_value = engine.FindStringKey("contextual_search_url");
    if (string_value) {
      contextual_search_url = *string_value;
    }
    string_value = engine.FindStringKey("logo_url");
    if (string_value) {
      logo_url = *string_value;
    }
    string_value = engine.FindStringKey("doodle_url");
    if (string_value) {
      doodle_url = *string_value;
    }
    string_value = engine.FindStringKey("search_url_post_params");
    if (string_value) {
      search_url_post_params = *string_value;
    }
    string_value = engine.FindStringKey("suggest_url_post_params");
    if (string_value) {
      suggest_url_post_params = *string_value;
    }
    string_value = engine.FindStringKey("image_url_post_params");
    if (string_value) {
      image_url_post_params = *string_value;
    }
    string_value = engine.FindStringKey("side_search_param");
    if (string_value) {
      side_search_param = *string_value;
    }
    string_value = engine.FindStringKey("preconnect_to_search_url");
    if (string_value) {
      preconnect_to_search_url = *string_value;
    }
    string_value = engine.FindStringKey("prefetch_likely_navigations");
    if (string_value) {
      prefetch_likely_navigations = *string_value;
    }

    return std::make_unique<TemplateURLData>(
        name, keyword, search_url, suggest_url, image_url, new_tab_url,
        contextual_search_url, logo_url, doodle_url, search_url_post_params,
        suggest_url_post_params, image_url_post_params, side_search_param,
        favicon_url, encoding, *alternate_urls,
        preconnect_to_search_url.compare("ALLOWED") == 0,
        prefetch_likely_navigations.compare("ALLOWED") == 0, *id);
  }
  return nullptr;
}

std::unique_ptr<TemplateURLData> TemplateURLDataFromStarterPackEngine(
    const TemplateURLStarterPackData::StarterPackEngine& engine) {
  auto turl = std::make_unique<TemplateURLData>();
  turl->SetShortName(l10n_util::GetStringUTF16(engine.name_message_id));
  turl->SetKeyword(u"@" + l10n_util::GetStringUTF16(engine.keyword_message_id));
  turl->SetURL(engine.search_url);
  turl->favicon_url = GURL(ToStringPiece(engine.favicon_url));
  turl->starter_pack_id = engine.id;
  turl->GenerateSyncGUID();
  turl->is_active = TemplateURLData::ActiveStatus::kTrue;

  return turl;
}
