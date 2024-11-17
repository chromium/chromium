// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_data_util.h"

#include <string>
#include <string_view>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

// Converts the C-style string `str` to a std::string_view making sure to avoid
// dereferencing nullptrs.
std::string_view ToStringView(const char* str) {
  return str ? std::string_view(str) : std::string_view();
}

std::u16string_view ToU16StringView(const char16_t* str) {
  return str ? std::u16string_view(str) : std::u16string_view();
}

}  // namespace

std::unique_ptr<TemplateURLData> TemplateURLDataFromDictionary(
    const base::Value::Dict& dict) {
  const std::string* search_url = dict.FindString(DefaultSearchManager::kURL);
  const std::string* keyword = dict.FindString(DefaultSearchManager::kKeyword);
  const std::string* short_name =
      dict.FindString(DefaultSearchManager::kShortName);
  // Check required TemplateURLData fields first.
  if (!search_url || !keyword || !short_name) {
    return nullptr;
  }

  auto result = std::make_unique<TemplateURLData>();
  result->SetKeyword(base::UTF8ToUTF16(*keyword));
  result->SetURL(*search_url);

  const std::string* id = dict.FindString(DefaultSearchManager::kID);
  if (id) {
    base::StringToInt64(*id, &result->id);
  }

  const std::string* string_value = nullptr;

  result->SetShortName(base::UTF8ToUTF16(*short_name));
  result->prepopulate_id = dict.FindInt(DefaultSearchManager::kPrepopulateID)
                               .value_or(result->prepopulate_id);
  result->starter_pack_id = dict.FindInt(DefaultSearchManager::kStarterPackId)
                                .value_or(result->starter_pack_id);
  string_value = dict.FindString(DefaultSearchManager::kSyncGUID);
  if (string_value) {
    result->sync_guid = *string_value;
  }
  string_value = dict.FindString(DefaultSearchManager::kSuggestionsURL);
  if (string_value) {
    result->suggestions_url = *string_value;
  }

  string_value = dict.FindString(DefaultSearchManager::kImageURL);
  if (string_value) {
    result->image_url = *string_value;
  }
  string_value = dict.FindString(DefaultSearchManager::kImageTranslateURL);
  if (string_value) {
    result->image_translate_url = *string_value;
  }
  string_value = dict.FindString(DefaultSearchManager::kNewTabURL);
  if (string_value) {
    result->new_tab_url = *string_value;
  }
  string_value = dict.FindString(DefaultSearchManager::kContextualSearchURL);
  if (string_value) {
    result->contextual_search_url = *string_value;
  }

  string_value = dict.FindString(DefaultSearchManager::kFaviconURL);
  if (string_value) {
    result->favicon_url = GURL(*string_value);
  }
  string_value = dict.FindString(DefaultSearchManager::kOriginatingURL);
  if (string_value) {
    result->originating_url = GURL(*string_value);
  }
  string_value = dict.FindString(DefaultSearchManager::kLogoURL);
  if (string_value) {
    result->logo_url = GURL(*string_value);
  }
  string_value = dict.FindString(DefaultSearchManager::kDoodleURL);
  if (string_value) {
    result->doodle_url = GURL(*string_value);
  }

  const std::string* search_url_post_params =
      dict.FindString(DefaultSearchManager::kSearchURLPostParams);
  if (search_url_post_params) {
    result->search_url_post_params = *search_url_post_params;
  }
  const std::string* suggestions_url_post_params =
      dict.FindString(DefaultSearchManager::kSuggestionsURLPostParams);
  if (suggestions_url_post_params) {
    result->suggestions_url_post_params = *suggestions_url_post_params;
  }
  const std::string* image_url_post_params =
      dict.FindString(DefaultSearchManager::kImageURLPostParams);
  if (image_url_post_params) {
    result->image_url_post_params = *image_url_post_params;
  }
  const std::string* side_search_param =
      dict.FindString(DefaultSearchManager::kSideSearchParam);
  if (side_search_param) {
    result->side_search_param = *side_search_param;
  }
  const std::string* side_image_search_param =
      dict.FindString(DefaultSearchManager::kSideImageSearchParam);
  if (side_image_search_param) {
    result->side_image_search_param = *side_image_search_param;
  }
  const std::string* image_translate_source_language_param_key =
      dict.FindString(
          DefaultSearchManager::kImageTranslateSourceLanguageParamKey);
  if (image_translate_source_language_param_key) {
    result->image_translate_source_language_param_key =
        *image_translate_source_language_param_key;
  }
  const std::string* image_translate_target_language_param_key =
      dict.FindString(
          DefaultSearchManager::kImageTranslateTargetLanguageParamKey);
  if (image_translate_target_language_param_key) {
    result->image_translate_target_language_param_key =
        *image_translate_target_language_param_key;
  }
  const std::string* image_search_branding_label =
      dict.FindString(DefaultSearchManager::kImageSearchBrandingLabel);
  if (image_search_branding_label) {
    result->image_search_branding_label =
        base::UTF8ToUTF16(*image_search_branding_label);
  }
  const base::Value::List* additional_params_list =
      dict.FindList(DefaultSearchManager::kSearchIntentParams);
  if (additional_params_list) {
    for (const auto& additional_param_value : *additional_params_list) {
      const auto* additional_param = additional_param_value.GetIfString();
      DCHECK(additional_param && !additional_param->empty());
      if (additional_param && !additional_param->empty()) {
        result->search_intent_params.push_back(*additional_param);
      }
    }
  }
  std::optional<bool> safe_for_autoreplace =
      dict.FindBool(DefaultSearchManager::kSafeForAutoReplace);
  if (safe_for_autoreplace) {
    result->safe_for_autoreplace = *safe_for_autoreplace;
  }

  std::string date_created_str;
  std::string last_modified_str;
  std::string last_visited_str;

  string_value = dict.FindString(DefaultSearchManager::kDateCreated);
  if (string_value) {
    date_created_str = *string_value;
  }
  string_value = dict.FindString(DefaultSearchManager::kLastModified);
  if (string_value) {
    last_modified_str = *string_value;
  }
  string_value = dict.FindString(DefaultSearchManager::kLastVisited);
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

  result->usage_count = dict.FindInt(DefaultSearchManager::kUsageCount)
                            .value_or(result->usage_count);

  const base::Value::List* alternate_urls =
      dict.FindList(DefaultSearchManager::kAlternateURLs);
  if (alternate_urls) {
    for (const auto& it : *alternate_urls) {
      if (it.is_string())
        result->alternate_urls.push_back(it.GetString());
    }
  }

  const base::Value::List* encodings =
      dict.FindList(DefaultSearchManager::kInputEncodings);
  if (encodings) {
    for (const auto& it : *encodings) {
      std::string encoding;
      if (it.is_string())
        result->input_encodings.push_back(it.GetString());
    }
  }

  result->created_by_policy = static_cast<TemplateURLData::CreatedByPolicy>(
      dict.FindInt(DefaultSearchManager::kCreatedByPolicy)
          .value_or(static_cast<int>(result->created_by_policy)));
  result->created_from_play_api =
      dict.FindBool(DefaultSearchManager::kCreatedFromPlayAPI)
          .value_or(result->created_from_play_api);
  result->featured_by_policy =
      dict.FindBool(DefaultSearchManager::kFeaturedByPolicy)
          .value_or(result->featured_by_policy);
  result->preconnect_to_search_url =
      dict.FindBool(DefaultSearchManager::kPreconnectToSearchUrl)
          .value_or(result->preconnect_to_search_url);
  result->prefetch_likely_navigations =
      dict.FindBool(DefaultSearchManager::kPrefetchLikelyNavigations)
          .value_or(result->prefetch_likely_navigations);
  result->is_active = static_cast<TemplateURLData::ActiveStatus>(
      dict.FindInt(DefaultSearchManager::kIsActive)
          .value_or(static_cast<int>(result->is_active)));
  result->enforced_by_policy =
      dict.FindBool(DefaultSearchManager::kEnforcedByPolicy)
          .value_or(result->enforced_by_policy);
  return result;
}

base::Value::Dict TemplateURLDataToDictionary(const TemplateURLData& data) {
  base::Value::Dict url_dict;
  url_dict.Set(DefaultSearchManager::kID, base::NumberToString(data.id));
  url_dict.Set(DefaultSearchManager::kShortName, data.short_name());
  url_dict.Set(DefaultSearchManager::kKeyword, data.keyword());
  url_dict.Set(DefaultSearchManager::kPrepopulateID, data.prepopulate_id);
  url_dict.Set(DefaultSearchManager::kStarterPackId, data.starter_pack_id);
  url_dict.Set(DefaultSearchManager::kSyncGUID, data.sync_guid);

  url_dict.Set(DefaultSearchManager::kURL, data.url());
  url_dict.Set(DefaultSearchManager::kSuggestionsURL, data.suggestions_url);
  url_dict.Set(DefaultSearchManager::kImageURL, data.image_url);
  url_dict.Set(DefaultSearchManager::kImageTranslateURL,
               data.image_translate_url);
  url_dict.Set(DefaultSearchManager::kNewTabURL, data.new_tab_url);
  url_dict.Set(DefaultSearchManager::kContextualSearchURL,
               data.contextual_search_url);
  url_dict.Set(DefaultSearchManager::kFaviconURL, data.favicon_url.spec());
  url_dict.Set(DefaultSearchManager::kOriginatingURL,
               data.originating_url.spec());
  url_dict.Set(DefaultSearchManager::kLogoURL, data.logo_url.spec());
  url_dict.Set(DefaultSearchManager::kDoodleURL, data.doodle_url.spec());

  url_dict.Set(DefaultSearchManager::kSearchURLPostParams,
               data.search_url_post_params);
  url_dict.Set(DefaultSearchManager::kSuggestionsURLPostParams,
               data.suggestions_url_post_params);
  url_dict.Set(DefaultSearchManager::kImageURLPostParams,
               data.image_url_post_params);
  url_dict.Set(DefaultSearchManager::kSideSearchParam, data.side_search_param);
  url_dict.Set(DefaultSearchManager::kSideImageSearchParam,
               data.side_image_search_param);
  url_dict.Set(DefaultSearchManager::kImageTranslateSourceLanguageParamKey,
               data.image_translate_source_language_param_key);
  url_dict.Set(DefaultSearchManager::kImageTranslateTargetLanguageParamKey,
               data.image_translate_target_language_param_key);
  url_dict.Set(DefaultSearchManager::kImageSearchBrandingLabel,
               data.image_search_branding_label);

  base::Value::List additional_params_list;
  for (const auto& additional_param : data.search_intent_params) {
    additional_params_list.Append(additional_param);
  }
  url_dict.Set(DefaultSearchManager::kSearchIntentParams,
               std::move(additional_params_list));

  url_dict.Set(DefaultSearchManager::kSafeForAutoReplace,
               data.safe_for_autoreplace);

  url_dict.Set(DefaultSearchManager::kDateCreated,
               base::NumberToString(data.date_created.ToInternalValue()));
  url_dict.Set(DefaultSearchManager::kLastModified,
               base::NumberToString(data.last_modified.ToInternalValue()));
  url_dict.Set(DefaultSearchManager::kLastVisited,
               base::NumberToString(data.last_visited.ToInternalValue()));
  url_dict.Set(DefaultSearchManager::kUsageCount, data.usage_count);

  base::Value::List alternate_urls;
  for (const auto& alternate_url : data.alternate_urls)
    alternate_urls.Append(alternate_url);

  url_dict.Set(DefaultSearchManager::kAlternateURLs, std::move(alternate_urls));

  base::Value::List encodings;
  for (const auto& input_encoding : data.input_encodings)
    encodings.Append(input_encoding);
  url_dict.Set(DefaultSearchManager::kInputEncodings, std::move(encodings));

  url_dict.Set(DefaultSearchManager::kCreatedByPolicy,
               static_cast<int>(data.created_by_policy));
  url_dict.Set(DefaultSearchManager::kCreatedFromPlayAPI,
               data.created_from_play_api);
  url_dict.Set(DefaultSearchManager::kFeaturedByPolicy,
               data.featured_by_policy);
  url_dict.Set(DefaultSearchManager::kPreconnectToSearchUrl,
               data.preconnect_to_search_url);
  url_dict.Set(DefaultSearchManager::kPrefetchLikelyNavigations,
               data.prefetch_likely_navigations);
  url_dict.Set(DefaultSearchManager::kIsActive,
               static_cast<int>(data.is_active));
  url_dict.Set(DefaultSearchManager::kEnforcedByPolicy,
               data.enforced_by_policy);
  return url_dict;
}

std::unique_ptr<TemplateURLData> TemplateURLDataFromPrepopulatedEngine(
    const TemplateURLPrepopulateData::PrepopulatedEngine& engine) {
  std::vector<std::string> search_intent_params;
  for (const auto* search_intent_param : engine.search_intent_params) {
    search_intent_params.emplace_back(search_intent_param);
  }

  base::Value::List alternate_urls;
  for (const auto* alternate_url : engine.alternate_urls) {
    alternate_urls.Append(std::string(alternate_url));
  }

  std::u16string image_search_branding_label =
      engine.image_search_branding_label ? engine.image_search_branding_label
                                         : std::u16string();

  return std::make_unique<TemplateURLData>(
      ToU16StringView(engine.name), ToU16StringView(engine.keyword),
      ToStringView(engine.search_url), ToStringView(engine.suggest_url),
      ToStringView(engine.image_url), ToStringView(engine.image_translate_url),
      ToStringView(engine.new_tab_url),
      ToStringView(engine.contextual_search_url), ToStringView(engine.logo_url),
      ToStringView(engine.doodle_url),
      ToStringView(engine.search_url_post_params),
      ToStringView(engine.suggest_url_post_params),
      ToStringView(engine.image_url_post_params),
      ToStringView(engine.side_search_param),
      ToStringView(engine.side_image_search_param),
      ToStringView(engine.image_translate_source_language_param_key),
      ToStringView(engine.image_translate_target_language_param_key),
      std::move(search_intent_params), ToStringView(engine.favicon_url),
      ToStringView(engine.encoding), image_search_branding_label,
      alternate_urls,
      ToStringView(engine.preconnect_to_search_url) == "ALLOWED",
      ToStringView(engine.prefetch_likely_navigations) == "ALLOWED", engine.id,
      engine.regulatory_extensions);
}

std::unique_ptr<TemplateURLData> TemplateURLDataFromOverrideDictionary(
    const base::Value::Dict& engine_dict) {
  const std::string* string_value = nullptr;

  std::u16string name;
  std::u16string keyword;
  std::string search_url;
  std::string favicon_url;
  std::string encoding;

  string_value = engine_dict.FindString("name");
  if (string_value) {
    name = base::UTF8ToUTF16(*string_value);
  }
  string_value = engine_dict.FindString("keyword");
  if (string_value) {
    keyword = base::UTF8ToUTF16(*string_value);
  }
  string_value = engine_dict.FindString("search_url");
  if (string_value) {
    search_url = *string_value;
  }
  string_value = engine_dict.FindString("favicon_url");
  if (string_value) {
    favicon_url = *string_value;
  }
  string_value = engine_dict.FindString("encoding");
  if (string_value) {
    encoding = *string_value;
  }
  std::optional<int> id = engine_dict.FindInt("id");

  // The following fields are required for each search engine configuration.
  if (!name.empty() && !keyword.empty() && !search_url.empty() &&
      !favicon_url.empty() && !encoding.empty() && id.has_value()) {
    // These fields are optional.
    base::Value::List empty_list;
    const base::Value::List* alternate_urls =
        engine_dict.FindList("alternate_urls");
    if (!alternate_urls)
      alternate_urls = &empty_list;

    std::string suggest_url;
    std::string image_url;
    std::string image_translate_url;
    std::string new_tab_url;
    std::string contextual_search_url;
    std::string logo_url;
    std::string doodle_url;
    std::string search_url_post_params;
    std::string suggest_url_post_params;
    std::string image_url_post_params;
    std::string side_search_param;
    std::string side_image_search_param;
    std::string image_translate_source_language_param_key;
    std::string image_translate_target_language_param_key;
    std::u16string image_search_branding_label;
    std::vector<std::string> search_intent_params;
    std::string preconnect_to_search_url;
    std::string prefetch_likely_navigations;

    string_value = engine_dict.FindString("suggest_url");
    if (string_value) {
      suggest_url = *string_value;
    }
    string_value = engine_dict.FindString("image_url");
    if (string_value) {
      image_url = *string_value;
    }
    string_value = engine_dict.FindString("image_translate_url");
    if (string_value) {
      image_translate_url = *string_value;
    }
    string_value = engine_dict.FindString("new_tab_url");
    if (string_value) {
      new_tab_url = *string_value;
    }
    string_value = engine_dict.FindString("contextual_search_url");
    if (string_value) {
      contextual_search_url = *string_value;
    }
    string_value = engine_dict.FindString("logo_url");
    if (string_value) {
      logo_url = *string_value;
    }
    string_value = engine_dict.FindString("doodle_url");
    if (string_value) {
      doodle_url = *string_value;
    }
    string_value = engine_dict.FindString("search_url_post_params");
    if (string_value) {
      search_url_post_params = *string_value;
    }
    string_value = engine_dict.FindString("suggest_url_post_params");
    if (string_value) {
      suggest_url_post_params = *string_value;
    }
    string_value = engine_dict.FindString("image_url_post_params");
    if (string_value) {
      image_url_post_params = *string_value;
    }
    string_value = engine_dict.FindString("side_search_param");
    if (string_value) {
      side_search_param = *string_value;
    }
    string_value = engine_dict.FindString("side_image_search_param");
    if (string_value) {
      side_image_search_param = *string_value;
    }
    string_value =
        engine_dict.FindString("image_translate_source_language_param_key");
    if (string_value) {
      image_translate_source_language_param_key = *string_value;
    }
    string_value =
        engine_dict.FindString("image_translate_target_language_param_key");
    if (string_value) {
      image_translate_target_language_param_key = *string_value;
    }
    string_value = engine_dict.FindString("image_search_branding_label");
    if (string_value) {
      image_search_branding_label = base::UTF8ToUTF16(*string_value);
    }
    const base::Value::List* additional_params_list =
        engine_dict.FindList(DefaultSearchManager::kSearchIntentParams);
    if (additional_params_list) {
      for (const auto& additional_param_value : *additional_params_list) {
        const auto* additional_param = additional_param_value.GetIfString();
        if (additional_param && !additional_param->empty()) {
          search_intent_params.push_back(*additional_param);
        }
      }
    }
    string_value = engine_dict.FindString("preconnect_to_search_url");
    if (string_value) {
      preconnect_to_search_url = *string_value;
    }
    string_value = engine_dict.FindString("prefetch_likely_navigations");
    if (string_value) {
      prefetch_likely_navigations = *string_value;
    }

    return std::make_unique<TemplateURLData>(
        name, keyword, search_url, suggest_url, image_url, image_translate_url,
        new_tab_url, contextual_search_url, logo_url, doodle_url,
        search_url_post_params, suggest_url_post_params, image_url_post_params,
        side_search_param, side_image_search_param,
        image_translate_source_language_param_key,
        image_translate_target_language_param_key,
        std::move(search_intent_params), favicon_url, encoding,
        image_search_branding_label, *alternate_urls,
        preconnect_to_search_url.compare("ALLOWED") == 0,
        prefetch_likely_navigations.compare("ALLOWED") == 0, *id,
        base::span<const TemplateURLData::RegulatoryExtension>());
  }
  return nullptr;
}

std::unique_ptr<TemplateURLData> TemplateURLDataFromStarterPackEngine(
    const TemplateURLStarterPackData::StarterPackEngine& engine) {
  auto turl = std::make_unique<TemplateURLData>();
  turl->SetShortName(l10n_util::GetStringUTF16(engine.name_message_id));
  turl->SetKeyword(u"@" + l10n_util::GetStringUTF16(engine.keyword_message_id));
  turl->SetURL(engine.search_url);
  turl->favicon_url = GURL(ToStringView(engine.favicon_url));
  turl->starter_pack_id = engine.id;
  turl->GenerateSyncGUID();
  turl->safe_for_autoreplace = true;
  turl->is_active = TemplateURLData::ActiveStatus::kTrue;

  return turl;
}
