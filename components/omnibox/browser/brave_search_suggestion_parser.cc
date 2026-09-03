// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/brave_search_suggestion_parser.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/omnibox_proto/entity_info.pb.h"
#include "third_party/omnibox_proto/navigational_intent.pb.h"
#include "third_party/omnibox_proto/types.pb.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace omnibox::brave_search {

namespace {

// Returns the vertical a suggestion belongs to, or an empty view when absent.
// `type` is only sent when the request asks for `rich_verticals`.
std::string_view GetVerticalType(const base::DictValue& suggestion) {
  const std::string* type = suggestion.FindString("type");
  return type ? *type : std::string_view();
}

// Returns whether the image at `image_url` can be shown for a suggestion.
//
// The native UI cannot render SVGs, so an image whose path names one is
// rejected, leaving the match to fall back to the default search icon. Note
// that this only recognizes an extension that is visible in the URL: an image
// served through a proxy that encodes the original URL -- as
// `imgs.search.brave.com` does -- cannot be identified here, so this is a
// best-effort check rather than a guarantee.
bool IsUsableImageUrl(std::string_view image_url) {
  const GURL url(image_url);
  // Do not blindly trust the URL coming from the server to be valid.
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  // Compare against the path so that a query or a reference fragment neither
  // hides the extension nor is mistaken for one.
  return !base::EndsWith(url.path(), ".svg",
                         base::CompareCase::INSENSITIVE_ASCII);
}

// Returns the answer of a `calculator` suggestion. It is a JSON number for
// arithmetic, but a string for results like unit conversions.
std::optional<std::u16string> GetCalculatorAnswer(
    const base::DictValue& suggestion) {
  if (const std::string* answer = suggestion.FindString("answer")) {
    return base::UTF8ToUTF16(*answer);
  }
  if (std::optional<double> answer = suggestion.FindDouble("answer")) {
    return base::NumberToString16(*answer);
  }
  return std::nullopt;
}

}  // namespace

void ParseSuggestResults(const base::ListValue& results_list,
                         const std::u16string& input_text,
                         int default_result_relevance,
                         bool is_keyword_result,
                         SearchSuggestionParser::Results* results) {
  results->verbatim_relevance = -1;
  results->field_trial_triggered = false;
  results->relevances_from_server = false;
  results->suggest_results.clear();
  results->navigation_results.clear();

  const std::u16string& trimmed_input =
      base::CollapseWhitespace(input_text, false);

  for (const auto& suggestion : results_list) {
    if (!suggestion.is_dict()) {
      continue;
    }
    const base::DictValue& suggestion_dict = suggestion.GetDict();
    const std::string* search_query = suggestion_dict.FindString("q");
    if (!search_query) {
      continue;
    }

    AutocompleteMatchType::Type match_type =
        AutocompleteMatchType::SEARCH_SUGGEST;
    omnibox::SuggestType suggest_type = omnibox::TYPE_QUERY;
    if (suggestion_dict.FindBool("is_entity").value_or(false)) {
      // Entities predate `type` and are flagged by `is_entity` instead.
      suggest_type = omnibox::TYPE_ENTITY;
      match_type = AutocompleteMatchType::SEARCH_SUGGEST_ENTITY;
    } else if (GetVerticalType(suggestion_dict) == "calculator") {
      suggest_type = omnibox::TYPE_CALCULATOR;
      match_type = AutocompleteMatchType::CALCULATOR;
    }
    // Verticals that are not handled above stay plain query suggestions.

    std::optional<omnibox::SuggestTemplateInfo> suggest_template_info;
    if (suggest_type == omnibox::TYPE_ENTITY) {
      suggest_template_info.emplace();
      suggest_template_info->set_style(omnibox::SuggestTemplateInfo::ENRICHED);
    }
    if (const std::string* name = suggestion_dict.FindString("name")) {
      if (!suggest_template_info) {
        suggest_template_info.emplace();
      }
      suggest_template_info->mutable_primary_text()->set_text(*name);
    }
    if (const std::string* image_url = suggestion_dict.FindString("img");
        image_url && IsUsableImageUrl(*image_url)) {
      if (!suggest_template_info) {
        suggest_template_info.emplace();
      }
      suggest_template_info->mutable_image()->set_url(*image_url);
    }

    std::u16string annotation;
    if (const std::string* description = suggestion_dict.FindString("desc");
        description && !description->empty()) {
      annotation = base::UTF8ToUTF16(*description);
      if (!suggest_template_info) {
        suggest_template_info.emplace();
      }
      suggest_template_info->mutable_secondary_text()->set_text(*description);
    }

    std::u16string suggestion_text;
    std::u16string match_contents;
    if (suggest_type == omnibox::TYPE_CALCULATOR) {
      std::optional<std::u16string> answer =
          GetCalculatorAnswer(suggestion_dict);
      if (!answer || answer->empty()) {
        // Nothing to display.
        continue;
      }
      // An annotation becomes the match description, which restores the
      // separator the desktop match cell suppresses for CALCULATOR -- the row
      // would read "<answer> - <description>".
      annotation.clear();
      // The suggestion is the answer, so accepting the match searches the text
      // the user typed. See BaseSearchProvider::CreateSearchSuggestion().
      suggestion_text = std::move(*answer);
      match_contents = suggestion_text;
      if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_DESKTOP) {
        // Desktop shows "<expression> = <answer>" on a single line, as it does
        // for the default response format.
        const std::string* expression =
            suggestion_dict.FindString("expression");
        match_contents = l10n_util::GetStringFUTF16(
            IDS_OMNIBOX_ONE_LINE_CALCULATOR_SUGGESTION_TEMPLATE,
            base::UTF8ToUTF16(expression ? *expression : *search_query),
            suggestion_text);
      }
    } else {
      suggestion_text = base::UTF8ToUTF16(*search_query);
      match_contents = suggestion_text;
    }

    results->suggest_results.emplace_back(
        suggestion_text, match_type, suggest_type,
        /*subtypes=*/std::vector<int>(), match_contents,
        /*match_contents_prefix=*/std::u16string(), annotation,
        /*deletion_url=*/std::string(), is_keyword_result,
        omnibox::NAV_INTENT_NONE, default_result_relevance,
        /*relevance_from_server=*/false, /*should_prefetch=*/false,
        /*should_prerender=*/false, trimmed_input,
        std::move(suggest_template_info));
  }
}

}  // namespace omnibox::brave_search
