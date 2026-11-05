// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggest_template_info_utils.h"

#include <string>

#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/formatted_string.pb.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"
#include "url/gurl.h"

namespace suggest_template_info {
namespace {

TEST(SuggestTemplateInfoUtilsTest, ACMatchClassificationToColorType) {
  EXPECT_EQ(
      internal::ACMatchClassificationToColorType(ACMatchClassification::NONE),
      omnibox::FormattedString::COLOR_SUGGEST_QUERY_TEXT);
  EXPECT_EQ(
      internal::ACMatchClassificationToColorType(ACMatchClassification::MATCH),
      omnibox::FormattedString::COLOR_SUGGEST_QUERY_TEXT);
  EXPECT_EQ(
      internal::ACMatchClassificationToColorType(ACMatchClassification::DIM),
      omnibox::FormattedString::COLOR_SUGGEST_HINT_TEXT);
  EXPECT_EQ(
      internal::ACMatchClassificationToColorType(ACMatchClassification::URL),
      omnibox::FormattedString::COLOR_SUGGEST_ACTION_HIGH);
  // URL wins over DIM when both are set.
  EXPECT_EQ(internal::ACMatchClassificationToColorType(
                ACMatchClassification::URL | ACMatchClassification::DIM),
            omnibox::FormattedString::COLOR_SUGGEST_ACTION_HIGH);
}

TEST(SuggestTemplateInfoUtilsTest, CreateFormattedStringFromClassification) {
  const std::u16string text = u"Google Search";
  const ACMatchClassifications classifications = {
      {0, ACMatchClassification::MATCH},
      {6, ACMatchClassification::NONE},
  };
  omnibox::FormattedString formatted_string =
      internal::CreateFormattedString(text, classifications);
  EXPECT_EQ(formatted_string.text(), "Google Search");
  ASSERT_EQ(formatted_string.fragments_size(), 2);

  EXPECT_EQ(formatted_string.fragments(0).start_index(), 0u);
  EXPECT_TRUE(formatted_string.fragments(0).is_bolded());
  EXPECT_EQ(formatted_string.fragments(0).color(),
            omnibox::FormattedString::COLOR_SUGGEST_QUERY_TEXT);

  EXPECT_EQ(formatted_string.fragments(1).start_index(), 6u);
  EXPECT_FALSE(formatted_string.fragments(1).is_bolded());
  EXPECT_EQ(formatted_string.fragments(1).color(),
            omnibox::FormattedString::COLOR_SUGGEST_QUERY_TEXT);
}

TEST(SuggestTemplateInfoUtilsTest, SecondaryTextPlacement) {
  {
    AutocompleteMatch match;
    EXPECT_EQ(
        internal::GetSecondaryTextPlacementFromMatch(match),
        omnibox::SuggestTemplateInfo::SECONDARY_TEXT_PLACEMENT_UNSPECIFIED);
  }
  {
    AutocompleteMatch match;
    match.image_url = GURL("https://example.com/image.png");
    EXPECT_EQ(internal::GetSecondaryTextPlacementFromMatch(match),
              omnibox::SuggestTemplateInfo::BELOW_PRIMARY_TEXT);
  }
  {
    // Non-empty image URL (even if invalid) triggers below-primary placement.
    AutocompleteMatch match;
    match.image_url = GURL("https://example.com:foo/image.png");
    EXPECT_FALSE(match.image_url.is_empty());
    EXPECT_FALSE(match.image_url.is_valid());
    EXPECT_EQ(internal::GetSecondaryTextPlacementFromMatch(match),
              omnibox::SuggestTemplateInfo::BELOW_PRIMARY_TEXT);
  }
  {
    AutocompleteMatch match;
    match.type = omnibox::AutocompleteMatchType::kCalculator;
    EXPECT_EQ(internal::GetSecondaryTextPlacementFromMatch(match),
              omnibox::SuggestTemplateInfo::BELOW_PRIMARY_TEXT);
  }
  {
    // Server placement overrides heuristics.
    AutocompleteMatch match;
    match.image_url = GURL("https://example.com/image.png");
    match.suggest_template = omnibox::SuggestTemplateInfo();
    match.suggest_template->set_secondary_text_placement(
        omnibox::SuggestTemplateInfo::IN_FRONT_OF_PRIMARY_TEXT);
    EXPECT_EQ(internal::GetSecondaryTextPlacementFromMatch(match),
              omnibox::SuggestTemplateInfo::IN_FRONT_OF_PRIMARY_TEXT);
  }
}

TEST(SuggestTemplateInfoUtilsTest, CreatePresentationTemplatePopulatesFields) {
  AutocompleteMatch match;
  match.type = omnibox::AutocompleteMatchType::kSearchSuggest;
  match.contents = u"weather";
  match.contents_class = {{0, ACMatchClassification::MATCH}};
  match.description = u"Sunny 72°F";
  match.description_class = {{0, ACMatchClassification::DIM}};

  omnibox::SuggestTemplateInfo template_info =
      CreatePresentationTemplate(match);

  // Match should not be mutated.
  EXPECT_FALSE(match.suggest_template.has_value());

  EXPECT_TRUE(template_info.has_primary_text());
  EXPECT_EQ(template_info.primary_text().text(), "weather");
  ASSERT_EQ(template_info.primary_text().fragments_size(), 1);
  EXPECT_TRUE(template_info.primary_text().fragments(0).is_bolded());

  EXPECT_TRUE(template_info.has_secondary_text());
  EXPECT_EQ(template_info.secondary_text().text(), "Sunny 72°F");
  ASSERT_EQ(template_info.secondary_text().fragments_size(), 1);
  EXPECT_EQ(template_info.secondary_text().fragments(0).color(),
            omnibox::FormattedString::COLOR_SUGGEST_HINT_TEXT);
}

TEST(SuggestTemplateInfoUtilsTest, CreatePresentationTemplateHandlesSwap) {
  AutocompleteMatch match;
  match.type = omnibox::AutocompleteMatchType::kSearchSuggest;
  match.swap_contents_and_description = true;
  match.contents = u"Subtitle description";
  match.contents_class = {{0, ACMatchClassification::DIM}};
  match.description = u"Primary title";
  match.description_class = {{0, ACMatchClassification::MATCH}};

  omnibox::SuggestTemplateInfo template_info =
      CreatePresentationTemplate(match);

  EXPECT_EQ(template_info.primary_text().text(), "Primary title");
  EXPECT_TRUE(template_info.primary_text().fragments(0).is_bolded());

  EXPECT_EQ(template_info.secondary_text().text(), "Subtitle description");
  EXPECT_EQ(template_info.secondary_text().fragments(0).color(),
            omnibox::FormattedString::COLOR_SUGGEST_HINT_TEXT);
}

TEST(SuggestTemplateInfoUtilsTest,
     CreatePresentationTemplatePreservesServerColors) {
  AutocompleteMatch match;
  match.contents = u"singer";
  match.contents_class = {{0, ACMatchClassification::DIM}};
  match.suggest_template = omnibox::SuggestTemplateInfo();
  auto* primary = match.suggest_template->mutable_primary_text();
  primary->set_text("singer");
  auto* frag = primary->add_fragments();
  frag->set_start_index(0);
  frag->set_color(omnibox::FormattedString::COLOR_SUGGEST_ACTION_HIGH);

  omnibox::SuggestTemplateInfo template_info =
      CreatePresentationTemplate(match);

  EXPECT_EQ(template_info.primary_text().text(), "singer");
  ASSERT_EQ(template_info.primary_text().fragments_size(), 1);
  EXPECT_EQ(template_info.primary_text().fragments(0).color(),
            omnibox::FormattedString::COLOR_SUGGEST_ACTION_HIGH);
}

TEST(SuggestTemplateInfoUtilsTest,
     CreatePresentationTemplatePreservesServerA11yText) {
  AutocompleteMatch match;
  match.contents = u"search result";
  match.contents_class = {{0, ACMatchClassification::NONE}};
  match.description = u"rating 5 stars";
  match.description_class = {{0, ACMatchClassification::DIM}};
  match.suggest_template = omnibox::SuggestTemplateInfo();
  match.suggest_template->mutable_primary_text()->set_text("search result");
  match.suggest_template->mutable_primary_text()->set_a11y_text(
      "search result, accessible description");
  match.suggest_template->mutable_secondary_text()->set_text("rating 5 stars");
  match.suggest_template->mutable_secondary_text()->set_a11y_text(
      "rated 5 out of 5 stars");

  omnibox::SuggestTemplateInfo template_info =
      CreatePresentationTemplate(match);

  EXPECT_EQ(template_info.primary_text().a11y_text(),
            "search result, accessible description");
  EXPECT_EQ(template_info.secondary_text().a11y_text(),
            "rated 5 out of 5 stars");
}

TEST(SuggestTemplateInfoUtilsTest,
     CreatePresentationTemplateValidatesImageUrl) {
  AutocompleteMatch match;
  match.contents = u"query";
  match.contents_class = {{0, ACMatchClassification::NONE}};
  // Invalid image URL should not produce an image.
  match.image_url = GURL("invalid-url");

  omnibox::SuggestTemplateInfo template_info =
      CreatePresentationTemplate(match);
  EXPECT_FALSE(template_info.has_image());

  // Valid image URL produces an image.
  match.image_url = GURL("https://example.com/valid.png");
  match.image_dominant_color = "#aabbcc";
  template_info = CreatePresentationTemplate(match);
  EXPECT_TRUE(template_info.has_image());
  EXPECT_EQ(template_info.image().url(), "https://example.com/valid.png");
  EXPECT_EQ(template_info.image().dominant_color(), "#aabbcc");

  // Empty dominant color is not set on the image proto.
  match.image_dominant_color.clear();
  template_info = CreatePresentationTemplate(match);
  EXPECT_TRUE(template_info.has_image());
  EXPECT_FALSE(template_info.image().has_dominant_color());
}

}  // namespace
}  // namespace suggest_template_info
