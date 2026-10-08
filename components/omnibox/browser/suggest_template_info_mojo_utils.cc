// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggest_template_info_mojo_utils.h"

#include <vector>

#include "base/containers/to_vector.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"

namespace suggest_template_info {

namespace {

// Converts `classifications` to their Mojo representation.
std::vector<mojom::ACMatchClassificationPtr> CreateClassifications(
    const ACMatchClassifications& classifications) {
  return base::ToVector(classifications,
                        [](const ACMatchClassification& classification) {
                          return mojom::ACMatchClassification::New(
                              classification.offset, classification.style);
                        });
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
  suggest_template->primary_text = swap ? match.description : match.contents;
  suggest_template->primary_text_class = CreateClassifications(
      swap ? match.description_class : match.contents_class);
  suggest_template->secondary_text = swap ? match.contents : match.description;
  suggest_template->secondary_text_class = CreateClassifications(
      swap ? match.contents_class : match.description_class);
  suggest_template->secondary_text_placement = GetSecondaryTextPlacement(match);
  suggest_template->image = GetImage(match);
  return suggest_template;
}

}  // namespace suggest_template_info
