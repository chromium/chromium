// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggest_template_info_mojo_utils.h"

#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"

namespace suggest_template_info {

namespace {

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
  suggest_template->secondary_text_placement = GetSecondaryTextPlacement(match);
  suggest_template->image = GetImage(match);
  return suggest_template;
}

}  // namespace suggest_template_info
