// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_tags_util.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"

namespace app_list {
namespace {

using CrosApiSearchResult = crosapi::mojom::SearchResult;

int ACMatchStyleToTagStyle(int styles) {
  int tag_styles = 0;
  if (styles & ACMatchClassification::URL)
    tag_styles |= ash::SearchResultTag::URL;
  if (styles & ACMatchClassification::MATCH)
    tag_styles |= ash::SearchResultTag::MATCH;
  if (styles & ACMatchClassification::DIM)
    tag_styles |= ash::SearchResultTag::DIM;

  return tag_styles;
}

}  // namespace

void ACMatchClassificationsToTags(const std::u16string& text,
                                  const ACMatchClassifications& text_classes,
                                  ChromeSearchResult::Tags* tags,
                                  const bool ignore_match) {
  int tag_styles = ash::SearchResultTag::NONE;
  size_t tag_start = 0;

  for (size_t i = 0; i < text_classes.size(); ++i) {
    const ACMatchClassification& text_class = text_classes[i];

    // Closes current tag.
    if (tag_styles != ash::SearchResultTag::NONE) {
      tags->push_back(
          ash::SearchResultTag(tag_styles, tag_start, text_class.offset));
      tag_styles = ash::SearchResultTag::NONE;
    }

    int style = text_class.style;
    if (ignore_match) {
      style &= ~ACMatchClassification::MATCH;
    }

    if (style == ACMatchClassification::NONE) {
      continue;
    }

    tag_start = text_class.offset;
    tag_styles = ACMatchStyleToTagStyle(style);
  }

  if (tag_styles != ash::SearchResultTag::NONE) {
    tags->push_back(ash::SearchResultTag(tag_styles, tag_start, text.length()));
  }
}

ChromeSearchResult::Tags CalculateTags(const std::u16string& query,
                                       const std::u16string& text) {
  ChromeSearchResult::Tags tags;
  AppendMatchTags(query, text, &tags);
  return tags;
}

void AppendMatchTags(const std::u16string& query,
                     const std::u16string& text,
                     ChromeSearchResult::Tags* tags) {
  const auto matches = FindTermMatches(query, text);
  const auto classes =
      ClassifyTermMatches(matches, text.length(),
                          /*match_style=*/ACMatchClassification::MATCH,
                          /*non_match_style=*/ACMatchClassification::NONE);

  ACMatchClassificationsToTags(text, classes, tags);
}

ash::SearchResultTags TagsForText(const std::u16string& text,
                                  CrosApiSearchResult::TextType type) {
  ash::SearchResultTags tags;
  const auto length = text.length();
  switch (type) {
    case CrosApiSearchResult::TextType::kPositive:
      tags.push_back(
          ash::SearchResultTag(ash::SearchResultTag::GREEN, 0, length));
      break;
    case CrosApiSearchResult::TextType::kNegative:
      tags.push_back(
          ash::SearchResultTag(ash::SearchResultTag::RED, 0, length));
      break;
    case CrosApiSearchResult::TextType::kUrl:
      tags.push_back(
          ash::SearchResultTag(ash::SearchResultTag::URL, 0, length));
      break;
    default:
      break;
  }
  return tags;
}

}  // namespace app_list
