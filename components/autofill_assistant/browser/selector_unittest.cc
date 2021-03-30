// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/selector.h"

#include "base/macros.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::UnorderedElementsAre;

namespace autofill_assistant {
namespace {

TEST(SelectorTest, Constructor_Simple) {
  Selector selector({"#test"});
  ASSERT_EQ(1, selector.proto.filters().size());
  EXPECT_EQ("#test", selector.proto.filters(0).css_selector());
}

TEST(SelectorTest, Constructor_WithIframe) {
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

TEST(SelectorTest, Comparison) {
  // Note that comparison tests cover < indirectly through ==, since a == b is
  // defined as !(a < b) && !(b < a). This makes sense, as what matters is that
  // there is an order but what the order is doesn't matter.

  EXPECT_FALSE(Selector({"a"}) == Selector({"b"}));
  EXPECT_TRUE(Selector({"a"}) == Selector({"a"}));
}

TEST(SelectorTest, SelectorInSet) {
  std::set<Selector> selectors;
  selectors.insert(Selector({"a"}));
  selectors.insert(Selector({"a"}));
  selectors.insert(Selector({"b"}));
  selectors.insert(Selector({"c"}));
  EXPECT_THAT(selectors, UnorderedElementsAre(Selector({"a"}), Selector({"b"}),
                                              Selector({"c"})));
}

TEST(SelectorTest, Comparison_PseudoType) {
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

TEST(SelectorTest, Comparison_Visibility) {
  EXPECT_FALSE(Selector({"a"}) == Selector({"a"}).MustBeVisible());
  EXPECT_TRUE(Selector({"a"}).MustBeVisible() ==
              Selector({"a"}).MustBeVisible());
}

TEST(SelectorTest, Comparison_NonEmptyBoundingBox) {
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

TEST(SelectorTest, Comparison_InnerText) {
  EXPECT_FALSE(Selector({"a"}).MatchingInnerText("a") ==
               Selector({"a"}).MatchingInnerText("b"));
  EXPECT_TRUE(Selector({"a"}).MatchingInnerText("a") ==
              Selector({"a"}).MatchingInnerText("a"));

  EXPECT_FALSE(Selector({"a"}).MatchingInnerText("a", false) ==
               Selector({"a"}).MatchingInnerText("a", true));
  EXPECT_TRUE(Selector({"a"}).MatchingInnerText("a", true) ==
              Selector({"a"}).MatchingInnerText("a", true));
}

TEST(SelectorTest, Comparison_Value) {
  EXPECT_FALSE(Selector({"a"}).MatchingValue("a") ==
               Selector({"a"}).MatchingValue("b"));
  EXPECT_TRUE(Selector({"a"}).MatchingValue("a") ==
              Selector({"a"}).MatchingValue("a"));

  EXPECT_FALSE(Selector({"a"}).MatchingValue("a", false) ==
               Selector({"a"}).MatchingValue("a", true));
  EXPECT_TRUE(Selector({"a"}).MatchingValue("a", true) ==
              Selector({"a"}).MatchingValue("a", true));
}

TEST(SelectorTest, Comparison_MatchCssSelector) {
  Selector a = Selector({"button"});
  a.proto.add_filters()->set_match_css_selector(".class1");
  Selector b = Selector({"button"});
  b.proto.add_filters()->set_match_css_selector(".class2");

  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a == a);
}

TEST(SelectorTest, Comparison_OnTop) {
  Selector a = Selector({"button"});
  a.proto.add_filters()->mutable_on_top();
  Selector b = Selector({"button"});

  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a == a);
}

TEST(SelectorTest, Comparison_Frames) {
  Selector ab({"a", "b"});
  EXPECT_EQ(ab, ab);

  Selector cb({"c", "b"});
  EXPECT_TRUE(cb == cb);
  EXPECT_FALSE(ab == cb);

  Selector b({"b"});
  EXPECT_TRUE(b == b);
  EXPECT_FALSE(ab == b);
}

TEST(SelectorTest, Comparison_MultipleFilters) {
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

}  // namespace
}  // namespace autofill_assistant
