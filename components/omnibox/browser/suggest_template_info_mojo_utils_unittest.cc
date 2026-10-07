// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggest_template_info_mojo_utils.h"

#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"
#include "url/gurl.h"

namespace suggest_template_info {
namespace {

omnibox::SuggestTemplateInfo CreateProtoWithPlacement(
    omnibox::SuggestTemplateInfo::SecondaryTextPlacement placement) {
  omnibox::SuggestTemplateInfo suggest_template;
  suggest_template.set_secondary_text_placement(placement);
  return suggest_template;
}

TEST(SuggestTemplateInfoMojoUtilsTest, UsesServerSecondaryTextPlacement) {
  {
    AutocompleteMatch match;
    match.suggest_template = CreateProtoWithPlacement(
        omnibox::SuggestTemplateInfo::BELOW_PRIMARY_TEXT);
    EXPECT_EQ(CreateSuggestTemplateInfo(match)->secondary_text_placement,
              mojom::SecondaryTextPlacement::kBelowPrimaryText);
  }
  // The server placement wins over the synthesized one, even for a match that
  // would otherwise be placed below (it has an image).
  {
    AutocompleteMatch match;
    match.image_url = GURL("https://example.com/image.png");
    match.suggest_template = CreateProtoWithPlacement(
        omnibox::SuggestTemplateInfo::IN_FRONT_OF_PRIMARY_TEXT);
    EXPECT_EQ(CreateSuggestTemplateInfo(match)->secondary_text_placement,
              mojom::SecondaryTextPlacement::kInFrontOfPrimaryText);
  }
}

TEST(SuggestTemplateInfoMojoUtilsTest, SynthesizesSecondaryTextPlacement) {
  // Plain matches keep the default placement, with or without a
  // SuggestTemplateInfo.
  {
    AutocompleteMatch match;
    EXPECT_EQ(CreateSuggestTemplateInfo(match)->secondary_text_placement,
              mojom::SecondaryTextPlacement::kUnspecified);
  }
  {
    AutocompleteMatch match;
    match.suggest_template = omnibox::SuggestTemplateInfo();
    EXPECT_EQ(CreateSuggestTemplateInfo(match)->secondary_text_placement,
              mojom::SecondaryTextPlacement::kUnspecified);
  }
  // Entity images.
  {
    AutocompleteMatch match;
    match.image_url = GURL("https://example.com/image.png");
    EXPECT_EQ(CreateSuggestTemplateInfo(match)->secondary_text_placement,
              mojom::SecondaryTextPlacement::kBelowPrimaryText);
  }
  // Calculator answers.
  {
    AutocompleteMatch match;
    match.type = omnibox::AutocompleteMatchType::kCalculator;
    EXPECT_EQ(CreateSuggestTemplateInfo(match)->secondary_text_placement,
              mojom::SecondaryTextPlacement::kBelowPrimaryText);
  }
  // Enterprise people suggestions.
  {
    AutocompleteMatch match;
    match.enterprise_search_aggregator_type =
        AutocompleteMatch::EnterpriseSearchAggregatorType::PEOPLE;
    EXPECT_EQ(CreateSuggestTemplateInfo(match)->secondary_text_placement,
              mojom::SecondaryTextPlacement::kBelowPrimaryText);
  }
  // An unspecified server placement falls back to the synthesized one.
  {
    AutocompleteMatch match;
    match.type = omnibox::AutocompleteMatchType::kCalculator;
    match.suggest_template = CreateProtoWithPlacement(
        omnibox::SuggestTemplateInfo::SECONDARY_TEXT_PLACEMENT_UNSPECIFIED);
    EXPECT_EQ(CreateSuggestTemplateInfo(match)->secondary_text_placement,
              mojom::SecondaryTextPlacement::kBelowPrimaryText);
  }
}

TEST(SuggestTemplateInfoMojoUtilsTest, PopulatesImageFromMatch) {
  // Matches without an image have no image.
  {
    AutocompleteMatch match;
    EXPECT_FALSE(CreateSuggestTemplateInfo(match)->image);
  }
  // The image URL and dominant color of the match are forwarded.
  {
    AutocompleteMatch match;
    match.image_url = GURL("https://example.com/image.png");
    match.image_dominant_color = "#757575";
    mojom::SuggestTemplateInfoPtr suggest_template =
        CreateSuggestTemplateInfo(match);
    ASSERT_TRUE(suggest_template->image);
    EXPECT_EQ(suggest_template->image->url,
              GURL("https://example.com/image.png"));
    EXPECT_EQ(suggest_template->image->dominant_color, "#757575");
  }
  // The image is read from the match rather than from its SuggestTemplateInfo,
  // so that images dropped by providers (e.g. ablated ones) are not shown.
  {
    AutocompleteMatch match;
    omnibox::SuggestTemplateInfo suggest_template;
    suggest_template.mutable_image()->set_url("https://example.com/image.png");
    suggest_template.mutable_image()->set_dominant_color("#757575");
    match.suggest_template = suggest_template;
    EXPECT_FALSE(CreateSuggestTemplateInfo(match)->image);
  }
  // A dominant color without an image URL is dropped.
  {
    AutocompleteMatch match;
    match.image_dominant_color = "#757575";
    EXPECT_FALSE(CreateSuggestTemplateInfo(match)->image);
  }
}

}  // namespace
}  // namespace suggest_template_info
