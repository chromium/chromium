// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggest_template_info_mojo_utils.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/fusebox_action.mojom.h"
#include "components/omnibox/browser/fusebox_action_mojo_utils.h"
#include "components/omnibox/browser/suggest_template_info_utils.h"
#include "third_party/omnibox_proto/formatted_string.pb.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"
#include "url/gurl.h"

namespace suggest_template_info {

namespace {

mojom::FormattedStringPtr CreateMojoFormattedString(
    const omnibox::FormattedString& proto) {
  static_assert(
      static_cast<int32_t>(omnibox::FormattedString::ColorType_MAX) ==
          static_cast<int32_t>(mojom::ColorType::kMaxValue),
      "ColorType enum values must match between the proto and the mojom.");
  std::vector<mojom::FormattedStringFragmentPtr> fragments;
  fragments.reserve(proto.fragments_size());
  for (const auto& fragment : proto.fragments()) {
    mojom::ColorType color = mojom::ColorType::kUnknown;
    if (fragment.has_color() &&
        omnibox::FormattedString::ColorType_IsValid(fragment.color())) {
      color = static_cast<mojom::ColorType>(fragment.color());
    }
    fragments.push_back(mojom::FormattedStringFragment::New(
        fragment.start_index(), color, fragment.is_bolded()));
  }
  return mojom::FormattedString::New(
      base::UTF8ToUTF16(proto.text()), std::move(fragments),
      proto.has_a11y_text()
          ? std::make_optional(base::UTF8ToUTF16(proto.a11y_text()))
          : std::nullopt);
}

mojom::SecondaryTextPlacement GetSecondaryTextPlacement(
    const omnibox::SuggestTemplateInfo& proto) {
  static_assert(
      static_cast<int32_t>(
          omnibox::SuggestTemplateInfo::SecondaryTextPlacement_MAX) ==
          static_cast<int32_t>(mojom::SecondaryTextPlacement::kMaxValue),
      "SecondaryTextPlacement enum values must match between proto and mojom.");
  if (proto.has_secondary_text_placement() &&
      omnibox::SuggestTemplateInfo::SecondaryTextPlacement_IsValid(
          proto.secondary_text_placement())) {
    return static_cast<mojom::SecondaryTextPlacement>(
        proto.secondary_text_placement());
  }
  return mojom::SecondaryTextPlacement::kUnspecified;
}

mojom::ImagePtr GetImage(const omnibox::SuggestTemplateInfo& proto) {
  if (!proto.has_image() || !proto.image().has_url()) {
    return nullptr;
  }
  GURL url(proto.image().url());
  if (!url.is_valid()) {
    return nullptr;
  }
  return mojom::Image::New(std::move(url), proto.image().dominant_color());
}

mojom::IconType GetTypeIcon(const omnibox::SuggestTemplateInfo& proto) {
  static_assert(
      static_cast<int32_t>(omnibox::SuggestTemplateInfo::IconType_MAX) ==
          static_cast<int32_t>(mojom::IconType::kMaxValue),
      "IconType enum values must match between the proto and the mojom.");
  if (!proto.has_type_icon() ||
      !omnibox::SuggestTemplateInfo::IconType_IsValid(proto.type_icon())) {
    return mojom::IconType::kIconTypeUnspecified;
  }
  return static_cast<mojom::IconType>(proto.type_icon());
}

mojom::SuggestStyle GetStyle(const omnibox::SuggestTemplateInfo& proto) {
  static_assert(static_cast<int32_t>(
                    omnibox::SuggestTemplateInfo::StandardSuggestStyle_MAX) ==
                    static_cast<int32_t>(mojom::SuggestStyle::kMaxValue),
                "SuggestStyle enum values must match between proto and mojom.");
  if (proto.has_style() &&
      omnibox::SuggestTemplateInfo::StandardSuggestStyle_IsValid(
          proto.style())) {
    return static_cast<mojom::SuggestStyle>(proto.style());
  }
  return mojom::SuggestStyle::kUnspecified;
}

fusebox_action::mojom::FuseboxActionPtr GetFuseboxAction(
    const omnibox::SuggestTemplateInfo& proto) {
  if (!proto.has_fusebox_action()) {
    return nullptr;
  }
  return fusebox_action::SyncFuseboxActionProtoToMojo(proto.fusebox_action());
}

mojom::SuggestTemplateInfoPtr CreateSuggestTemplateInfoFromProto(
    const omnibox::SuggestTemplateInfo& proto) {
  auto suggest_template = mojom::SuggestTemplateInfo::New();
  if (proto.has_primary_text()) {
    suggest_template->primary_text =
        CreateMojoFormattedString(proto.primary_text());
  } else {
    // primary_text is non-nullable in mojom; provide an empty fallback.
    suggest_template->primary_text = mojom::FormattedString::New();
  }
  if (proto.has_secondary_text() && !proto.secondary_text().text().empty()) {
    suggest_template->secondary_text =
        CreateMojoFormattedString(proto.secondary_text());
  }
  suggest_template->secondary_text_placement = GetSecondaryTextPlacement(proto);
  suggest_template->image = GetImage(proto);
  suggest_template->style = GetStyle(proto);
  suggest_template->type_icon = GetTypeIcon(proto);
  suggest_template->fusebox_action = GetFuseboxAction(proto);
  return suggest_template;
}

}  // namespace

mojom::SuggestTemplateInfoPtr CreateSuggestTemplateInfo(
    const AutocompleteMatch& match) {
  return CreateSuggestTemplateInfoFromProto(CreatePresentationTemplate(match));
}

}  // namespace suggest_template_info
