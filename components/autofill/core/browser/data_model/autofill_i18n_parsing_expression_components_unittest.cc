// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_i18n_parsing_expression_components.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::Eq;
using testing::Optional;
using testing::Pair;
using testing::UnorderedElementsAre;

namespace autofill::i18n_model_definition {

TEST(AutofillI18nParsingStructures, Decomposition) {
  Decomposition decomposition("(?P<foo>\\w+)", true, true);
  EXPECT_THAT(decomposition.Parse("aaa"),
              Optional(ElementsAre(Pair("foo", "aaa"))));

  EXPECT_THAT(decomposition.Parse("aaa aaa"), Eq(std::nullopt));
}

TEST(AutofillI18nParsingStructures, DecompositionAnchoringDisabled) {
  Decomposition decomposition("(?P<foo>\\w+)", false, false);

  EXPECT_THAT(decomposition.Parse("aaa"),
              Optional(ElementsAre(Pair("foo", "aaa"))));

  // If anchoring is disabled we do match the regex in this case.
  EXPECT_THAT(decomposition.Parse("aaa aaa"),
              Optional(ElementsAre(Pair("foo", "aaa"))));
}

TEST(AutofillI18nParsingStructures, DecompositionCascade) {
  Decomposition decomposition1("(?P<foo>.*a+)", true, true);
  Decomposition decomposition2("(?P<foo__2>.*b+)", true, true);
  const std::vector<const AutofillParsingProcess*> alternatives = {
      &decomposition1, &decomposition2};
  DecompositionCascade cascade("^1", alternatives);

  EXPECT_THAT(cascade.Parse("1aaa"),
              Optional(ElementsAre(Pair("foo", "1aaa"))));

  // It also checks whether the version suffix is removed.
  EXPECT_THAT(cascade.Parse("1bbb"),
              Optional(ElementsAre(Pair("foo", "1bbb"))));

  // The condition (a "1" at the beginning is violated), therefore, we don't
  // return anything.
  EXPECT_THAT(cascade.Parse("bbb"), Eq(std::nullopt));
}

TEST(AutofillI18nParsingStructures, ExtractPart) {
  ExtractPart extract_part("^1", "(?:prefix(?P<NAME_MIDDLE>[_a]+)suffix)");

  EXPECT_THAT(extract_part.Parse("1prefix_a_suffix"),
              Optional(ElementsAre(Pair("NAME_MIDDLE", "_a_"))));
}

TEST(AutofillI18nParsingStructures, ExtractParts) {
  ExtractPart part1("",
                    "(?:house number\\s+(?P<ADDRESS_HOME_HOUSE_NUMBER>\\d+))");
  ExtractPart part2("", "(?:apartment\\s+(?P<ADDRESS_HOME_APT_NUM>\\d+))");
  const std::vector<const ExtractPart*> parts_ptr = {&part1, &part2};
  ExtractParts extract_parts("2$", parts_ptr);

  EXPECT_THAT(
      extract_parts.Parse("1 house number 1 apartment 2"),
      Optional(UnorderedElementsAre(Pair("ADDRESS_HOME_HOUSE_NUMBER", "1"),
                                    Pair("ADDRESS_HOME_APT_NUM", "2"))));
}

TEST(AutofillI18nParsingStructures, RemoveVersionSuffix) {
  ExtractPart part("",
                   "(?:floor\\s+(?P<ADDRESS_HOME_FLOOR__1>\\d+)|(?P<ADDRESS_"
                   "HOME_FLOOR__2>\\d+)(?:st|nd|rd|th) floor)");

  EXPECT_THAT(part.Parse("3rd floor"),
              Optional(ElementsAre(Pair("ADDRESS_HOME_FLOOR", "3"))));
  EXPECT_THAT(part.Parse("floor 4"),
              Optional(ElementsAre(Pair("ADDRESS_HOME_FLOOR", "4"))));
}

}  // namespace autofill::i18n_model_definition
