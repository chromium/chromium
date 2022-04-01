// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/selector.h"

#include "base/containers/flat_set.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::UnorderedElementsAre;

TEST(SelectorTest, ConstructorSimple) {
  Selector selector({"#test"});
  ASSERT_EQ(1, selector.proto.filters().size());
  EXPECT_EQ("#test", selector.proto.filters(0).css_selector());
}

TEST(SelectorTest, ConstructorWithIframe) {
  Selector selector({"#frame", "#test"});
  ASSERT_EQ(4, selector.proto.filters().size());
  EXPECT_EQ("#frame", selector.proto.filters(0).css_selector());
  EXPECT_EQ(SelectorProto::Filter::kNthMatch,
            selector.proto.filters(1).filter_case());
  EXPECT_EQ(0, selector.proto.filters(1).nth_match().index());
  EXPECT_EQ(SelectorProto::Filter::kEnterFrame,
            selector.proto.filters(2).filter_case());
  EXPECT_EQ("#test", selector.proto.filters(3).css_selector());
}

TEST(SelectorTest, FromProto) {
  SelectorProto proto;
  proto.add_filters()->set_css_selector("#test");

  EXPECT_EQ(Selector({"#test"}), Selector(proto));
}

TEST(SelectorTest, EmptyCheckCssSelector) {
  EXPECT_TRUE(Selector().empty());
  EXPECT_FALSE(Selector({"#test"}).empty());
}

TEST(SelectorTest, EmptyCheckSelectorWithSemanticInformation) {
  SelectorProto semantic_only_proto;
  semantic_only_proto.mutable_semantic_information();
  EXPECT_FALSE(Selector(semantic_only_proto).empty());

  Selector semantic_and_css({"#test"});
  semantic_and_css.proto.mutable_semantic_information();
  EXPECT_FALSE(semantic_and_css.empty());
}

TEST(SelectorTest, EmptyCheckCssAndSemanticComparison) {
  SelectorProto semantic_only_proto;
  semantic_only_proto.mutable_semantic_information()
      ->set_check_matches_css_element(true);
  EXPECT_TRUE(Selector(semantic_only_proto).empty());

  Selector semantic_and_css({"#test"});
  semantic_and_css.proto.mutable_semantic_information()
      ->set_check_matches_css_element(true);
  EXPECT_FALSE(semantic_and_css.empty());
}

TEST(SelectorTest, Comparison) {
  // Note that comparison tests cover < indirectly through ==, since a == b is
  // defined as !(a < b) && !(b < a). This makes sense, as what matters is that
  // there is an order but what the order is doesn't matter.

  EXPECT_FALSE(Selector({"a"}) == Selector({"b"}));
  EXPECT_TRUE(Selector({"a"}) == Selector({"a"}));
}

TEST(SelectorTest, SelectorInSet) {
  base::flat_set<Selector> selectors;
  selectors.insert(Selector({"a"}));
  selectors.insert(Selector({"a"}));
  selectors.insert(Selector({"b"}));
  selectors.insert(Selector({"c"}));
  EXPECT_THAT(selectors, UnorderedElementsAre(Selector({"a"}), Selector({"b"}),
                                              Selector({"c"})));
}

TEST(SelectorTest, ComparisonPseudoType) {
  EXPECT_FALSE(Selector({"a"}).SetPseudoType(PseudoType::BEFORE) ==
               Selector({"a"}).SetPseudoType(PseudoType::AFTER));
  EXPECT_FALSE(Selector({"a"}).SetPseudoType(PseudoType::BEFORE) ==
               Selector({"a"}).SetPseudoType(PseudoType::AFTER));
  EXPECT_FALSE(Selector({"b"}) ==
               Selector({"a"}).SetPseudoType(PseudoType::BEFORE));
  EXPECT_FALSE(Selector({"b"}) ==
               Selector({"a"}).SetPseudoType(PseudoType::BEFORE));
  EXPECT_TRUE(Selector({"a"}).SetPseudoType(PseudoType::BEFORE) ==
              Selector({"a"}).SetPseudoType(PseudoType::BEFORE));
}

TEST(SelectorTest, ComparisonVisibility) {
  EXPECT_FALSE(Selector({"a"}) == Selector({"a"}).MustBeVisible());
  EXPECT_TRUE(Selector({"a"}).MustBeVisible() ==
              Selector({"a"}).MustBeVisible());
}

TEST(SelectorTest, ComparisonNonEmptyBoundingBox) {
  Selector has_bounding_box_default = Selector({"a"});
  has_bounding_box_default.proto.add_filters()->mutable_bounding_box();

  Selector has_bounding_box_explicit = Selector({"a"});
  has_bounding_box_explicit.proto.add_filters()
      ->mutable_bounding_box()
      ->set_require_nonempty(false);

  Selector has_nonempty_bounding_box = Selector({"a"});
  has_nonempty_bounding_box.proto.add_filters()
      ->mutable_bounding_box()
      ->set_require_nonempty(true);

  EXPECT_FALSE(has_bounding_box_default == has_nonempty_bounding_box);
  EXPECT_FALSE(has_bounding_box_explicit == has_nonempty_bounding_box);
  EXPECT_TRUE(has_bounding_box_default == has_bounding_box_explicit);
}

TEST(SelectorTest, ComparisonInnerText) {
  EXPECT_FALSE(Selector({"a"}).MatchingInnerText("a") ==
               Selector({"a"}).MatchingInnerText("b"));
  EXPECT_TRUE(Selector({"a"}).MatchingInnerText("a") ==
              Selector({"a"}).MatchingInnerText("a"));

  EXPECT_FALSE(Selector({"a"}).MatchingInnerText("a", false) ==
               Selector({"a"}).MatchingInnerText("a", true));
  EXPECT_TRUE(Selector({"a"}).MatchingInnerText("a", true) ==
              Selector({"a"}).MatchingInnerText("a", true));
}

TEST(SelectorTest, ComparisonValue) {
  EXPECT_FALSE(Selector({"a"}).MatchingValue("a") ==
               Selector({"a"}).MatchingValue("b"));
  EXPECT_TRUE(Selector({"a"}).MatchingValue("a") ==
              Selector({"a"}).MatchingValue("a"));

  EXPECT_FALSE(Selector({"a"}).MatchingValue("a", false) ==
               Selector({"a"}).MatchingValue("a", true));
  EXPECT_TRUE(Selector({"a"}).MatchingValue("a", true) ==
              Selector({"a"}).MatchingValue("a", true));
}

TEST(SelectorTest, ComparisonMatchCssSelector) {
  Selector a = Selector({"button"});
  a.proto.add_filters()->set_match_css_selector(".class1");
  Selector b = Selector({"button"});
  b.proto.add_filters()->set_match_css_selector(".class2");

  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a == a);
}

TEST(SelectorTest, ComparisonOnTop) {
  Selector a = Selector({"button"});
  a.proto.add_filters()->mutable_on_top();
  Selector b = Selector({"button"});

  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a == a);
}

TEST(SelectorTest, ComparisonFrames) {
  Selector ab({"a", "b"});
  EXPECT_EQ(ab, ab);

  Selector cb({"c", "b"});
  EXPECT_TRUE(cb == cb);
  EXPECT_FALSE(ab == cb);

  Selector b({"b"});
  EXPECT_TRUE(b == b);
  EXPECT_FALSE(ab == b);
}

TEST(SelectorTest, ComparisonMultipleFilters) {
  Selector abcdef;
  abcdef.proto.add_filters()->set_css_selector("abc");
  abcdef.proto.add_filters()->set_css_selector("def");

  Selector abcdef2;
  abcdef2.proto.add_filters()->set_css_selector("abc");
  abcdef2.proto.add_filters()->set_css_selector("def");
  EXPECT_EQ(abcdef, abcdef2);

  Selector defabc;
  defabc.proto.add_filters()->set_css_selector("def");
  defabc.proto.add_filters()->set_css_selector("abc");
  EXPECT_TRUE(defabc == defabc);
  EXPECT_FALSE(abcdef == defabc);

  Selector abc;
  abc.proto.add_filters()->set_css_selector("abc");
  EXPECT_TRUE(abc == abc);
  EXPECT_FALSE(abcdef == abc);
}

TEST(SelectorTest, ComparisonPropertyFilterTextFilter) {
  Selector text_filter;
  auto* filter = text_filter.proto.add_filters();
  filter->mutable_property()->set_property("innerText");
  filter->mutable_property()->mutable_text_filter()->set_case_sensitive(true);
  filter->mutable_property()->mutable_text_filter()->set_re2(".*");

  Selector text_filter_copy(text_filter.proto);
  EXPECT_TRUE(text_filter == text_filter_copy);

  Selector text_filter_property = text_filter;
  text_filter_property.proto.mutable_filters(0)
      ->mutable_property()
      ->set_property("value");
  EXPECT_FALSE(text_filter == text_filter_property);

  Selector text_filter_case = text_filter;
  text_filter_case.proto.mutable_filters(0)
      ->mutable_property()
      ->mutable_text_filter()
      ->set_case_sensitive(false);
  EXPECT_FALSE(text_filter == text_filter_case);

  Selector text_filter_re2 = text_filter;
  text_filter_re2.proto.mutable_filters(0)
      ->mutable_property()
      ->mutable_text_filter()
      ->set_re2("^$");
  EXPECT_FALSE(text_filter == text_filter_re2);
}

TEST(SelectorTest, ComparisonPropertyFilterAutofillValueRegexp) {
  Selector autofill_filter;
  auto* filter = autofill_filter.proto.add_filters();
  filter->mutable_property()->set_property("innerText");
  filter->mutable_property()
      ->mutable_autofill_value_regexp()
      ->mutable_profile()
      ->set_identifier("profile");
  filter->mutable_property()
      ->mutable_autofill_value_regexp()
      ->mutable_value_expression_re2()
      ->set_case_sensitive(true);
  filter->mutable_property()
      ->mutable_autofill_value_regexp()
      ->mutable_value_expression_re2()
      ->mutable_value_expression()
      ->add_chunk()
      ->set_text("chunk");

  Selector autofill_filter_copy(autofill_filter.proto);
  EXPECT_TRUE(autofill_filter == autofill_filter_copy);

  Selector autofill_filter_property = autofill_filter;
  autofill_filter_property.proto.mutable_filters(0)
      ->mutable_property()
      ->set_property("value");
  EXPECT_FALSE(autofill_filter == autofill_filter_property);

  Selector autofill_filter_profile = autofill_filter;
  autofill_filter_profile.proto.mutable_filters(0)
      ->mutable_property()
      ->mutable_autofill_value_regexp()
      ->mutable_profile()
      ->set_identifier("other");
  EXPECT_FALSE(autofill_filter == autofill_filter_profile);

  Selector autofill_filter_case = autofill_filter;
  autofill_filter_case.proto.mutable_filters(0)
      ->mutable_property()
      ->mutable_autofill_value_regexp()
      ->mutable_value_expression_re2()
      ->set_case_sensitive(false);
  EXPECT_FALSE(autofill_filter == autofill_filter_case);

  Selector autofill_filter_chunk = autofill_filter;
  autofill_filter_chunk.proto.mutable_filters(0)
      ->mutable_property()
      ->mutable_autofill_value_regexp()
      ->mutable_value_expression_re2()
      ->mutable_value_expression()
      ->mutable_chunk(0)
      ->set_text("text");
  EXPECT_FALSE(autofill_filter == autofill_filter_chunk);
}

}  // namespace
}  // namespace autofill_assistant
