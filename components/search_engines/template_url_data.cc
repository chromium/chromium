// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_data.h"

#include <string_view>

#include "base/check.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_map.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/regulatory_extension_type.h"

namespace {

// Returns a GUID used for sync, which is random except for built-in search
// engines. The latter benefit from using a deterministic GUID, to make sure
// sync doesn't incur in duplicates for prepopulated engines.
std::string GenerateGUID(int prepopulate_id, int starter_pack_id) {
  // We compute a GUID deterministically given |prepopulate_id| or
  // |starter_pack_id|, using an arbitrary base GUID.
  std::string guid;
  // IDs above 1000 are reserved for distribution custom engines.
  if (prepopulate_id > 0 && prepopulate_id <= 1000) {
    guid = base::StringPrintf("485bf7d3-0215-45af-87dc-538868%06d",
                              prepopulate_id);
  } else if (starter_pack_id > 0) {
    guid = base::StringPrintf("ec205736-edd7-4022-a9a3-b431fc%06d",
                              starter_pack_id);
  } else {
    guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  }

  DCHECK(base::Uuid::ParseCaseInsensitive(guid).is_valid());
  return guid;
}

}  // namespace

TemplateURLData::TemplateURLData()
    : safe_for_autoreplace(false),
      id(0),
      date_created(base::Time::Now()),
      last_modified(base::Time::Now()),
      created_by_policy(CreatedByPolicy::kNoPolicy),
      enforced_by_policy(false),
      created_from_play_api(false),
      usage_count(0),
      prepopulate_id(0),
      sync_guid(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      keyword_(u"dummy"),
      url_("x") {}

TemplateURLData::TemplateURLData(const TemplateURLData& other) = default;

TemplateURLData& TemplateURLData::operator=(const TemplateURLData& other) =
    default;

TemplateURLData::TemplateURLData(
    std::u16string_view name,
    std::u16string_view keyword,
    std::string_view search_url,
    std::string_view suggest_url,
    std::string_view image_url,
    std::string_view image_translate_url,
    std::string_view new_tab_url,
    std::string_view contextual_search_url,
    std::string_view logo_url,
    std::string_view doodle_url,
    std::string_view search_url_post_params,
    std::string_view suggest_url_post_params,
    std::string_view image_url_post_params,
    std::string_view side_search_param,
    std::string_view side_image_search_param,
    std::string_view image_translate_source_language_param_key,
    std::string_view image_translate_target_language_param_key,
    std::vector<std::string> search_intent_params,
    std::string_view favicon_url,
    std::string_view encoding,
    std::u16string_view image_search_branding_label,
    const base::Value::List& alternate_urls_list,
    bool preconnect_to_search_url,
    bool prefetch_likely_navigations,
    int prepopulate_id,
    const base::span<const TemplateURLData::RegulatoryExtension>&
        reg_extensions)
    : suggestions_url(suggest_url),
      image_url(image_url),
      image_translate_url(image_translate_url),
      new_tab_url(new_tab_url),
      contextual_search_url(contextual_search_url),
      logo_url(logo_url),
      doodle_url(doodle_url),
      search_url_post_params(search_url_post_params),
      suggestions_url_post_params(suggest_url_post_params),
      image_url_post_params(image_url_post_params),
      side_search_param(side_search_param),
      side_image_search_param(side_image_search_param),
      image_translate_source_language_param_key(
          image_translate_source_language_param_key),
      image_translate_target_language_param_key(
          image_translate_target_language_param_key),
      image_search_branding_label(image_search_branding_label),
      search_intent_params(search_intent_params),
      favicon_url(favicon_url),
      safe_for_autoreplace(true),
      id(0),
      created_by_policy(CreatedByPolicy::kNoPolicy),
      enforced_by_policy(false),
      created_from_play_api(false),
      usage_count(0),
      prepopulate_id(prepopulate_id),
      sync_guid(GenerateGUID(prepopulate_id, 0)),
      preconnect_to_search_url(preconnect_to_search_url),
      prefetch_likely_navigations(prefetch_likely_navigations) {
  SetShortName(name);
  SetKeyword(keyword);
  SetURL(std::string(search_url));
  input_encodings.push_back(std::string(encoding));
  for (const auto& entry : alternate_urls_list) {
    const std::string* alternate_url = entry.GetIfString();
    DCHECK(alternate_url && !alternate_url->empty());
    if (alternate_url) {
      alternate_urls.push_back(*alternate_url);
    }
  }

  regulatory_extensions = base::MakeFlatMap<
      RegulatoryExtensionType,
      raw_ptr<const TemplateURLData::RegulatoryExtension, CtnExperimental>>(
      reg_extensions, {}, [](const TemplateURLData::RegulatoryExtension& a) {
        return std::pair<RegulatoryExtensionType,
                         raw_ptr<const TemplateURLData::RegulatoryExtension,
                                 CtnExperimental>>(a.variant, &a);
      });
  DCHECK_EQ(regulatory_extensions.size(), reg_extensions.size());
}

TemplateURLData::~TemplateURLData() = default;

void TemplateURLData::SetShortName(std::u16string_view short_name) {
  // Remove tabs, carriage returns, and the like, as they can corrupt
  // how the short name is displayed.
  short_name_ = base::CollapseWhitespace(short_name, true);
}

void TemplateURLData::SetKeyword(std::u16string_view keyword) {
  DCHECK(!keyword.empty());

  // Case sensitive keyword matching is confusing. As such, we force all
  // keywords to be lower case.
  keyword_ = base::i18n::ToLower(keyword);

  // The omnibox doesn't properly handle search keywords with whitespace.
  base::RemoveChars(keyword_, base::kWhitespaceUTF16, &keyword_);
}

void TemplateURLData::SetURL(const std::string& url) {
  DCHECK(!url.empty());
  url_ = url;
}

void TemplateURLData::GenerateSyncGUID() {
  sync_guid = GenerateGUID(prepopulate_id, starter_pack_id);
}

size_t TemplateURLData::EstimateMemoryUsage() const {
  size_t res = 0;

  res += base::trace_event::EstimateMemoryUsage(suggestions_url);
  res += base::trace_event::EstimateMemoryUsage(image_url);
  res += base::trace_event::EstimateMemoryUsage(new_tab_url);
  res += base::trace_event::EstimateMemoryUsage(contextual_search_url);
  res += base::trace_event::EstimateMemoryUsage(logo_url);
  res += base::trace_event::EstimateMemoryUsage(doodle_url);
  res += base::trace_event::EstimateMemoryUsage(search_url_post_params);
  res += base::trace_event::EstimateMemoryUsage(suggestions_url_post_params);
  res += base::trace_event::EstimateMemoryUsage(image_url_post_params);
  res += base::trace_event::EstimateMemoryUsage(side_search_param);
  res += base::trace_event::EstimateMemoryUsage(side_image_search_param);
  res += base::trace_event::EstimateMemoryUsage(favicon_url);
  res += base::trace_event::EstimateMemoryUsage(image_search_branding_label);
  res += base::trace_event::EstimateMemoryUsage(originating_url);
  res += base::trace_event::EstimateMemoryUsage(input_encodings);
  res += base::trace_event::EstimateMemoryUsage(sync_guid);
  res += base::trace_event::EstimateMemoryUsage(alternate_urls);
  res += base::trace_event::EstimateMemoryUsage(short_name_);
  res += base::trace_event::EstimateMemoryUsage(keyword_);
  res += base::trace_event::EstimateMemoryUsage(url_);

  return res;
}
