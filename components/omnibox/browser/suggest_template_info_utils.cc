// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggest_template_info_utils.h"

#include <cstdint>
#include <string>

#include "base/containers/flat_map.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "third_party/omnibox_proto/formatted_string.pb.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"
#include "url/gurl.h"

namespace suggest_template_info {

namespace internal {

omnibox::FormattedString::ColorType ACMatchClassificationToColorType(
    int style) {
  if (style & ACMatchClassification::URL) {
    return omnibox::FormattedString::COLOR_SUGGEST_ACTION_HIGH;
  }
  if (style & ACMatchClassification::DIM) {
    return omnibox::FormattedString::COLOR_SUGGEST_HINT_TEXT;
  }
  return omnibox::FormattedString::COLOR_SUGGEST_QUERY_TEXT;
}

omnibox::FormattedString CreateFormattedString(
    const std::u16string& text,
    const ACMatchClassifications& classifications,
    const omnibox::FormattedString& suggest_template_text) {
  base::flat_map<size_t, omnibox::FormattedString::ColorType>
      suggest_template_colors;
  const bool text_matches =
      base::UTF8ToUTF16(suggest_template_text.text()) == text;
  // If `suggest_template_text` holds the same text, a fragment takes the color
  // of its fragment that starts at the same offset, if that has one. The parser
  // builds the classifications from those fragments, so their offsets line up.
  // The text has to match since dedupe and other changes to the match don't
  // update SuggestTemplateInfo.
  if (text_matches) {
    for (const auto& fragment : suggest_template_text.fragments()) {
      if (fragment.has_start_index() &&
          fragment.color() != omnibox::FormattedString::COLOR_UNKNOWN) {
        suggest_template_colors.emplace(fragment.start_index(),
                                        fragment.color());
      }
    }
  }

  omnibox::FormattedString formatted_string;
  formatted_string.set_text(base::UTF16ToUTF8(text));
  if (text_matches && suggest_template_text.has_a11y_text()) {
    formatted_string.set_a11y_text(suggest_template_text.a11y_text());
  }

  for (const auto& classification : classifications) {
    auto* fragment = formatted_string.add_fragments();
    fragment->set_start_index(
        base::checked_cast<uint32_t>(classification.offset));
    // Use the ACMatchClassification as a fallback since it's built
    // from the server values but contains a complete mapping.
    const auto it = suggest_template_colors.find(classification.offset);
    const auto color =
        it != suggest_template_colors.end()
            ? it->second
            : ACMatchClassificationToColorType(classification.style);
    fragment->set_color(color);
    fragment->set_is_bolded(
        (classification.style & ACMatchClassification::MATCH) != 0);
  }
  return formatted_string;
}

omnibox::SuggestTemplateInfo::SecondaryTextPlacement
GetSecondaryTextPlacementFromMatch(const AutocompleteMatch& match) {
  if (match.suggest_template &&
      match.suggest_template->has_secondary_text_placement() &&
      match.suggest_template->secondary_text_placement() !=
          omnibox::SuggestTemplateInfo::SECONDARY_TEXT_PLACEMENT_UNSPECIFIED) {
    return match.suggest_template->secondary_text_placement();
  }

  // Calculator answers and enterprise people suggestions place
  // their secondary text below the primary text.
  if (!match.image_url.is_empty() ||
      match.type == omnibox::AutocompleteMatchType::kCalculator ||
      match.enterprise_search_aggregator_type ==
          AutocompleteMatch::EnterpriseSearchAggregatorType::PEOPLE) {
    return omnibox::SuggestTemplateInfo::BELOW_PRIMARY_TEXT;
  }

  return omnibox::SuggestTemplateInfo::SECONDARY_TEXT_PLACEMENT_UNSPECIFIED;
}

}  // namespace internal

omnibox::SuggestTemplateInfo CreatePresentationTemplate(
    const AutocompleteMatch& match) {
  const omnibox::SuggestTemplateInfo& original_template =
      match.suggest_template ? *match.suggest_template
                             : omnibox::SuggestTemplateInfo::default_instance();
  omnibox::SuggestTemplateInfo template_info = original_template;

  // Resolve `swap_contents_and_description` here so the UI can always render
  // `contents` as the primary text and `description` as the secondary text.
  // NOTE: We read in the contents and description from the class on purpose
  // instead of the SuggestTemplateInfo proto fields. Dedupe keeps those in sync
  // with the text (see `AutocompleteMatch::UpgradeMatchWithPropertiesFrom()`),
  // but other transformations that happen to the match during the autocomplete
  // lifecycle don't, e.g. the location signaling match's description.
  const bool swap = match.swap_contents_and_description;
  const ACMatchClassifications& primary_text_class =
      swap ? match.description_class : match.contents_class;
  const ACMatchClassifications& secondary_text_class =
      swap ? match.contents_class : match.description_class;

  const std::u16string& primary_text =
      swap ? match.description : match.contents;
  *template_info.mutable_primary_text() =
      internal::CreateFormattedString(primary_text, primary_text_class,
                                      swap ? original_template.secondary_text()
                                           : original_template.primary_text());

  const std::u16string& secondary_text =
      swap ? match.contents : match.description;
  if (!secondary_text.empty()) {
    *template_info.mutable_secondary_text() = internal::CreateFormattedString(
        secondary_text, secondary_text_class,
        swap ? original_template.primary_text()
             : original_template.secondary_text());
  } else {
    template_info.clear_secondary_text();
  }

  const auto placement = internal::GetSecondaryTextPlacementFromMatch(match);
  if (placement !=
      omnibox::SuggestTemplateInfo::SECONDARY_TEXT_PLACEMENT_UNSPECIFIED) {
    template_info.set_secondary_text_placement(placement);
  } else {
    template_info.clear_secondary_text_placement();
  }

  if (match.image_url.is_valid()) {
    auto* image = template_info.mutable_image();
    image->set_url(match.image_url.spec());
    if (!match.image_dominant_color.empty()) {
      image->set_dominant_color(match.image_dominant_color);
    }
  } else {
    template_info.clear_image();
  }

  return template_info;
}

}  // namespace suggest_template_info
