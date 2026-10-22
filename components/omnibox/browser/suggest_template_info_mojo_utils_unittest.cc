// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggest_template_info_mojo_utils.h"

#include <array>
#include <cstdint>

#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/fusebox_action.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"
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

TEST(SuggestTemplateInfoMojoUtilsTest, PopulatesTypeIconFromSuggestTemplate) {
  // Matches without a SuggestTemplateInfo have no type icon.
  {
    AutocompleteMatch match;
    EXPECT_EQ(CreateSuggestTemplateInfo(match)->type_icon,
              mojom::IconType::kIconTypeUnspecified);
  }
  // The type icon from the server is forwarded.
  {
    AutocompleteMatch match;
    omnibox::SuggestTemplateInfo suggest_template;
    suggest_template.set_type_icon(
        omnibox::SuggestTemplateInfo::SEARCH_LOOP_WITH_SPARKLE);
    match.suggest_template = suggest_template;
    EXPECT_EQ(CreateSuggestTemplateInfo(match)->type_icon,
              mojom::IconType::kSearchLoopWithSparkle);
  }
}

TEST(SuggestTemplateInfoMojoUtilsTest,
     PopulatesFuseboxActionFromSuggestTemplate) {
  // Matches without a FuseboxAction from the server have none.
  {
    AutocompleteMatch match;
    EXPECT_FALSE(CreateSuggestTemplateInfo(match)->fusebox_action);
    match.suggest_template = omnibox::SuggestTemplateInfo();
    EXPECT_FALSE(CreateSuggestTemplateInfo(match)->fusebox_action);
  }
  // The FuseboxAction from the server is forwarded.
  {
    AutocompleteMatch match;
    omnibox::SuggestTemplateInfo suggest_template;
    suggest_template.mutable_fusebox_action()->set_preselected_tool(
        omnibox::TOOL_MODE_DEEP_SEARCH);
    suggest_template.mutable_fusebox_action()->set_query_action_override(
        omnibox::SuggestTemplateInfo::FuseboxAction::QUERY_ACTION_PASTE);
    match.suggest_template = suggest_template;
    mojom::SuggestTemplateInfoPtr suggest_template_info =
        CreateSuggestTemplateInfo(match);
    ASSERT_TRUE(suggest_template_info->fusebox_action);
    EXPECT_EQ(suggest_template_info->fusebox_action->preselected_tool,
              omnibox::TOOL_MODE_DEEP_SEARCH);
    EXPECT_EQ(suggest_template_info->fusebox_action->query_action_override,
              fusebox_action::mojom::QueryActionOverride::kPaste);
  }
}

TEST(SuggestTemplateInfoMojoUtilsTest, PopulatesTextFromMatch) {
  AutocompleteMatch match;
  match.contents = u"example.com";
  match.contents_class = {{0, ACMatchClassification::URL}};
  match.description = u"Example Title";
  match.description_class = {{0, ACMatchClassification::MATCH}};

  // The contents are the primary text and the description is the secondary
  // text.
  {
    mojom::SuggestTemplateInfoPtr suggest_template =
        CreateSuggestTemplateInfo(match);
    EXPECT_EQ(suggest_template->primary_text->text, u"example.com");
    ASSERT_EQ(suggest_template->primary_text->fragments.size(), 1u);
    EXPECT_EQ(suggest_template->primary_text->fragments[0]->color,
              mojom::ColorType::kSuggestActionHigh);
    EXPECT_EQ(suggest_template->secondary_text->text, u"Example Title");
    ASSERT_EQ(suggest_template->secondary_text->fragments.size(), 1u);
    EXPECT_TRUE(suggest_template->secondary_text->fragments[0]->is_bolded);
  }
  // `swap_contents_and_description` is resolved here, and the styling travels
  // with its text.
  {
    match.swap_contents_and_description = true;
    mojom::SuggestTemplateInfoPtr suggest_template =
        CreateSuggestTemplateInfo(match);
    EXPECT_EQ(suggest_template->primary_text->text, u"Example Title");
    ASSERT_EQ(suggest_template->primary_text->fragments.size(), 1u);
    EXPECT_TRUE(suggest_template->primary_text->fragments[0]->is_bolded);
    EXPECT_EQ(suggest_template->secondary_text->text, u"example.com");
    ASSERT_EQ(suggest_template->secondary_text->fragments.size(), 1u);
    EXPECT_EQ(suggest_template->secondary_text->fragments[0]->color,
              mojom::ColorType::kSuggestActionHigh);
    EXPECT_FALSE(suggest_template->secondary_text->fragments[0]->is_bolded);
  }
}

TEST(SuggestTemplateInfoMojoUtilsTest, LeavesEmptySecondaryTextNull) {
  // A match without a description has no secondary text.
  {
    AutocompleteMatch match;
    match.contents = u"example.com";
    mojom::SuggestTemplateInfoPtr suggest_template =
        CreateSuggestTemplateInfo(match);
    EXPECT_EQ(suggest_template->primary_text->text, u"example.com");
    EXPECT_FALSE(suggest_template->secondary_text);
  }
  // With `swap_contents_and_description`, the secondary text is the contents.
  {
    AutocompleteMatch match;
    match.description = u"Example Title";
    match.swap_contents_and_description = true;
    mojom::SuggestTemplateInfoPtr suggest_template =
        CreateSuggestTemplateInfo(match);
    EXPECT_EQ(suggest_template->primary_text->text, u"Example Title");
    EXPECT_FALSE(suggest_template->secondary_text);
  }
}

TEST(SuggestTemplateInfoMojoUtilsTest, ConvertsClassificationsToFragments) {
  AutocompleteMatch match;
  match.contents = u"0123456789";
  match.contents_class = {
      {0, ACMatchClassification::NONE},
      {1, ACMatchClassification::MATCH},
      {2, ACMatchClassification::DIM},
      {3, ACMatchClassification::URL},
      {4, ACMatchClassification::URL | ACMatchClassification::MATCH},
      {5, ACMatchClassification::DIM | ACMatchClassification::MATCH},
      {6, ACMatchClassification::URL | ACMatchClassification::DIM},
      {7, ACMatchClassification::TOOLBELT},
  };

  mojom::SuggestTemplateInfoPtr suggest_template =
      CreateSuggestTemplateInfo(match);
  const mojom::FormattedString& primary_text = *suggest_template->primary_text;
  EXPECT_EQ(primary_text.text, u"0123456789");
  EXPECT_FALSE(primary_text.a11y_text);

  struct Expected {
    uint32_t start_index;
    mojom::ColorType color;
    bool is_bolded;
  };
  const auto kExpected = std::to_array<Expected>({
      {0, mojom::ColorType::kSuggestQueryText, false},
      {1, mojom::ColorType::kSuggestQueryText, true},
      {2, mojom::ColorType::kSuggestHintText, false},
      {3, mojom::ColorType::kSuggestActionHigh, false},
      {4, mojom::ColorType::kSuggestActionHigh, true},
      {5, mojom::ColorType::kSuggestHintText, true},
      {6, mojom::ColorType::kSuggestActionHigh, false},
      {7, mojom::ColorType::kSuggestQueryText, false},
  });
  // Each classification becomes exactly one fragment, even when neighboring
  // classifications have the same style. MATCH is converted to `is_bolded`.
  // URL and DIM pick the color, URL wins when both are set, and other styles
  // get the query text color.
  ASSERT_EQ(primary_text.fragments.size(), kExpected.size());
  for (size_t i = 0; i < kExpected.size(); ++i) {
    SCOPED_TRACE(i);
    const mojom::FormattedStringFragment& fragment = *primary_text.fragments[i];
    EXPECT_EQ(fragment.start_index, kExpected[i].start_index);
    EXPECT_EQ(fragment.color, kExpected[i].color);
    EXPECT_EQ(fragment.is_bolded, kExpected[i].is_bolded);
  }
}

TEST(SuggestTemplateInfoMojoUtilsTest, KeepsClassificationOffsets) {
  // Offsets are UTF-16 offsets, like the classification offsets. The emoji
  // takes two code units.
  {
    AutocompleteMatch match;
    match.contents = u"\U0001F600 hello";
    match.contents_class = {{0, ACMatchClassification::NONE},
                            {3, ACMatchClassification::MATCH}};
    mojom::SuggestTemplateInfoPtr suggest_template =
        CreateSuggestTemplateInfo(match);
    ASSERT_EQ(suggest_template->primary_text->fragments.size(), 2u);
    EXPECT_EQ(suggest_template->primary_text->fragments[0]->start_index, 0u);
    EXPECT_EQ(suggest_template->primary_text->fragments[1]->start_index, 3u);
  }
  // A first offset other than 0 is kept as is. The WebUI renders the text
  // before it unstyled, like it does for classifications.
  {
    AutocompleteMatch match;
    match.contents = u"hello world";
    match.contents_class = {{6, ACMatchClassification::MATCH}};
    mojom::SuggestTemplateInfoPtr suggest_template =
        CreateSuggestTemplateInfo(match);
    ASSERT_EQ(suggest_template->primary_text->fragments.size(), 1u);
    EXPECT_EQ(suggest_template->primary_text->fragments[0]->start_index, 6u);
    EXPECT_TRUE(suggest_template->primary_text->fragments[0]->is_bolded);
  }
  // No classifications give no fragments, i.e. unstyled text.
  {
    AutocompleteMatch match;
    match.description = u"description";
    mojom::SuggestTemplateInfoPtr suggest_template =
        CreateSuggestTemplateInfo(match);
    EXPECT_EQ(suggest_template->secondary_text->text, u"description");
    EXPECT_TRUE(suggest_template->secondary_text->fragments.empty());
  }
}

TEST(SuggestTemplateInfoMojoUtilsTest, UsesServerStyle) {
  // Matches without a server-provided style have an unspecified style.
  {
    AutocompleteMatch match;
    EXPECT_EQ(CreateSuggestTemplateInfo(match)->style,
              mojom::SuggestStyle::kUnspecified);
  }
  {
    AutocompleteMatch match;
    omnibox::SuggestTemplateInfo suggest_template;
    suggest_template.set_style(omnibox::SuggestTemplateInfo::RICH_IMAGE);
    match.suggest_template = suggest_template;
    EXPECT_EQ(CreateSuggestTemplateInfo(match)->style,
              mojom::SuggestStyle::kRichImage);
  }
}

}  // namespace
}  // namespace suggest_template_info
