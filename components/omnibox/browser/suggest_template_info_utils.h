// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SUGGEST_TEMPLATE_INFO_UTILS_H_
#define COMPONENTS_OMNIBOX_BROWSER_SUGGEST_TEMPLATE_INFO_UTILS_H_

#include <string>

#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "third_party/omnibox_proto/formatted_string.pb.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"

struct AutocompleteMatch;

namespace suggest_template_info {

namespace internal {

// Returns the color token for text with the classification `style`. If both
// URL and DIM are set, URL wins. Other styles get COLOR_SUGGEST_QUERY_TEXT.
omnibox::FormattedString::ColorType ACMatchClassificationToColorType(int style);

// Converts `text` and its `classifications` to a FormattedString protobuf. Each
// classification becomes one fragment. The MATCH style is converted to
// `is_bolded`, and the other styles to a semantic ColorType.
//
// If `suggest_template_text` holds the same text, a fragment takes the color
// of its fragment that starts at the same offset, if that has one. The parser
// builds the classifications from those fragments, so their offsets line up.
// The text has to match since dedupe and other changes to the match don't
// update SuggestTemplateInfo.
//
// If `suggest_template_text` has `a11y_text`, it is preserved on the returned
// FormattedString only when the text is unchanged.
omnibox::FormattedString CreateFormattedString(
    const std::u16string& text,
    const ACMatchClassifications& classifications,
    const omnibox::FormattedString& suggest_template_text =
        omnibox::FormattedString::default_instance());

// Returns where to place the secondary text of `match` relative to its primary
// text. The placement from the server-provided SuggestTemplateInfo takes
// precedence. Otherwise, it is synthesized for matches whose presentation is
// not (yet) described by a SuggestTemplateInfo.
omnibox::SuggestTemplateInfo::SecondaryTextPlacement
GetSecondaryTextPlacementFromMatch(const AutocompleteMatch& match);

}  // namespace internal

// Returns a complete, normalized SuggestTemplateInfo presentation template for
// `match` without modifying `match`.
//
// Note:
// - The returned `primary_text` and `secondary_text` are in final display
//   order, with `swap_contents_and_description` already resolved. Downstream
//   renderers must not swap them again.
// - If `match.suggest_template` is present, its server fields (such as style,
//   type_icon, and fusebox_action) are preserved.
// - Server `a11y_text` is preserved only when the text is unchanged.
// - Server secondary text is dropped when the post-swap secondary text is
// empty.
// - Server image is ignored unless `match.image_url.is_valid()`, which
//   intentionally respects image ablation and filtering.
omnibox::SuggestTemplateInfo CreatePresentationTemplate(
    const AutocompleteMatch& match);

}  // namespace suggest_template_info

#endif  // COMPONENTS_OMNIBOX_BROWSER_SUGGEST_TEMPLATE_INFO_UTILS_H_
