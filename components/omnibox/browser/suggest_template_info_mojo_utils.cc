// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggest_template_info_mojo_utils.h"

#include <cstdint>
#include <optional>
#include <string>

#include "base/containers/to_vector.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/fusebox_action.mojom.h"
#include "components/omnibox/browser/fusebox_action_mojo_utils.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"

namespace suggest_template_info {

namespace {

// Returns the color for text with the classification `style`. If both URL and
// DIM are set, URL wins. Other styles get kSuggestQueryText.
mojom::ColorType ACMatchClassificationToColorType(int style) {
  if (style & ACMatchClassification::URL) {
    return mojom::ColorType::kSuggestActionHigh;
  }
  if (style & ACMatchClassification::DIM) {
    return mojom::ColorType::kSuggestHintText;
  }
  return mojom::ColorType::kSuggestQueryText;
}

// Converts `text` and its `classifications` to a FormattedString. Each
// classification becomes one fragment. The MATCH style is converted to
// `is_bolded`, and the other styles to a color.
mojom::FormattedStringPtr CreateFormattedString(
    const std::u16string& text,
    const ACMatchClassifications& classifications) {
  return mojom::FormattedString::New(
      text,
      base::ToVector(
          classifications,
          [](const ACMatchClassification& classification) {
            const bool is_bolded =
                (classification.style & ACMatchClassification::MATCH) != 0;
            return mojom::FormattedStringFragment::New(
                classification.offset,
                ACMatchClassificationToColorType(classification.style),
                is_bolded);
          }),
      /*a11y_text=*/std::nullopt);
}

// Returns where to place the secondary text of `match` relative to its primary
// text. The placement from the server-provided SuggestTemplateInfo takes
// precedence. Otherwise, it is synthesized for matches whose presentation is
// not (yet) described by a SuggestTemplateInfo.
mojom::SecondaryTextPlacement GetSecondaryTextPlacement(
    const AutocompleteMatch& match) {
  if (match.suggest_template) {
    switch (match.suggest_template->secondary_text_placement()) {
      case omnibox::SuggestTemplateInfo::IN_FRONT_OF_PRIMARY_TEXT:
        return mojom::SecondaryTextPlacement::kInFrontOfPrimaryText;
      case omnibox::SuggestTemplateInfo::BELOW_PRIMARY_TEXT:
        return mojom::SecondaryTextPlacement::kBelowPrimaryText;
      default:
        break;
    }
  }

  // Calculator answers and enterprise people suggestions place
  // their secondary text below the primary text.
  if (!match.image_url.is_empty() ||
      match.type == omnibox::AutocompleteMatchType::kCalculator ||
      match.enterprise_search_aggregator_type ==
          AutocompleteMatch::EnterpriseSearchAggregatorType::PEOPLE) {
    return mojom::SecondaryTextPlacement::kBelowPrimaryText;
  }

  return mojom::SecondaryTextPlacement::kUnspecified;
}

// Returns the image to display with `match`, or null if it has none.
mojom::ImagePtr GetImage(const AutocompleteMatch& match) {
  if (!match.image_url.is_valid()) {
    return nullptr;
  }
  return mojom::Image::New(match.image_url, match.image_dominant_color);
}

// Returns the icon from the server-provided SuggestTemplateInfo of `match`, or
// kIconTypeUnspecified if it has none.
mojom::IconType GetTypeIcon(const AutocompleteMatch& match) {
  static_assert(
      static_cast<int32_t>(omnibox::SuggestTemplateInfo::IconType_MAX) ==
          static_cast<int32_t>(mojom::IconType::kMaxValue),
      "IconType enum values must match between the proto and the mojom.");
  if (!match.suggest_template ||
      !omnibox::SuggestTemplateInfo::IconType_IsValid(
          match.suggest_template->type_icon())) {
    return mojom::IconType::kIconTypeUnspecified;
  }
  return static_cast<mojom::IconType>(match.suggest_template->type_icon());
}

// Returns the FuseboxAction from the server-provided SuggestTemplateInfo of
// `match`, or null if it has none.
fusebox_action::mojom::FuseboxActionPtr GetFuseboxAction(
    const AutocompleteMatch& match) {
  if (!match.suggest_template ||
      !match.suggest_template->has_fusebox_action()) {
    return nullptr;
  }
  return fusebox_action::SyncFuseboxActionProtoToMojo(
      match.suggest_template->fusebox_action());
}

}  // namespace

mojom::SuggestTemplateInfoPtr CreateSuggestTemplateInfo(
    const AutocompleteMatch& match) {
  auto suggest_template = mojom::SuggestTemplateInfo::New();
  // Resolve `swap_contents_and_description` here so the UI can always render
  // `contents` as the primary text and `description` as the secondary text.
  // NOTE: We read in the contents and description from the class on purpose
  // instead of the SuggestTemplateInfo proto fields. This is because the
  // SuggestTemplateInfo fields aren't updated during the match deduping
  // process and for other transformations that happen to the match during
  // the autocomplete lifecycle.
  const bool swap = match.swap_contents_and_description;
  const ACMatchClassifications& primary_text_class =
      swap ? match.description_class : match.contents_class;
  const ACMatchClassifications& secondary_text_class =
      swap ? match.contents_class : match.description_class;
  suggest_template->primary_text = CreateFormattedString(
      swap ? match.description : match.contents, primary_text_class);
  const std::u16string& secondary_text =
      swap ? match.contents : match.description;
  if (!secondary_text.empty()) {
    suggest_template->secondary_text =
        CreateFormattedString(secondary_text, secondary_text_class);
  }
  suggest_template->secondary_text_placement = GetSecondaryTextPlacement(match);
  suggest_template->image = GetImage(match);
  if (match.suggest_template && match.suggest_template->has_style()) {
    suggest_template->style =
        static_cast<mojom::SuggestStyle>(match.suggest_template->style());
  }
  suggest_template->type_icon = GetTypeIcon(match);
  suggest_template->fusebox_action = GetFuseboxAction(match);
  return suggest_template;
}

}  // namespace suggest_template_info
