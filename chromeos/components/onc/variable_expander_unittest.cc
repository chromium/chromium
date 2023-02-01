// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/onc/variable_expander.h"

#include "base/check_deref.h"
#include "base/files/file_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::variable_expander {

TEST(VariableExpanderTest, DoesNothingWithoutVariables) {
  VariableExpander expander({{"machine_name", "chromebook"}});
  std::string str = "String without variable";
  EXPECT_TRUE(expander.ExpandString(&str));
  EXPECT_EQ(str, "String without variable");
}

TEST(VariableExpanderTest, ExpandsFullVariables) {
  VariableExpander expander({{"machine_name", "chromebook"}});
  std::string str = "${machine_name}";
  EXPECT_TRUE(expander.ExpandString(&str));
  EXPECT_EQ(str, "chromebook");
}

TEST(VariableExpanderTest, ExpandsSubstringsWithPos) {
  VariableExpander expander({{"machine_name", "chromebook"}});
  std::string str = "${machine_name,6}";
  EXPECT_TRUE(expander.ExpandString(&str));
  EXPECT_EQ(str, "book");
}

TEST(VariableExpanderTest, ExpandsSubstringsWithPosAnsCount) {
  VariableExpander expander({{"machine_name", "chromebook"}});
  std::string str = "${machine_name,3,5}";
  EXPECT_TRUE(expander.ExpandString(&str));
  EXPECT_EQ(str, "omebo");
}

TEST(VariableExpanderTest, ExpandsMultipleVariables) {
  VariableExpander expander({{"machine_name", "chromebook"}});
  std::string str = "I run ${machine_name,0,6} on my ${machine_name}";
  EXPECT_TRUE(expander.ExpandString(&str));
  EXPECT_EQ(str, "I run chrome on my chromebook");
}

TEST(VariableExpanderTest, ExpandsAllTheGoodVariables) {
  VariableExpander expander({{"machine_name", "chromebook"}});
  std::string str = "I run ${machine_nameBAD} on my ${machine_name}";
  EXPECT_FALSE(expander.ExpandString(&str));
  EXPECT_EQ(str, "I run ${machine_nameBAD} on my chromebook");
}

TEST(VariableExpanderTest, ExpandsDifferentVariables) {
  VariableExpander expander({{"food", "bananas"}, {"sauce", "bbq"}});
  std::string str = "I like to eat ${food} with ${sauce} sauce";
  EXPECT_TRUE(expander.ExpandString(&str));
  EXPECT_EQ(str, "I like to eat bananas with bbq sauce");
}

TEST(VariableExpanderTest, ExpandsEmptyIfPosTooLarge) {
  VariableExpander expander({{"machine_name", "chromebook"}});
  std::string str = "${machine_name,20}";
  EXPECT_TRUE(expander.ExpandString(&str));
  EXPECT_EQ(str, "");
}

TEST(VariableExpanderTest, ExpandsFullIfCountTooLarge) {
  VariableExpander expander({{"machine_name", "chromebook"}});
  std::string str = "${machine_name,6,20}";
  EXPECT_TRUE(expander.ExpandString(&str));
  EXPECT_EQ(str, "book");
}

TEST(VariableExpanderTest, FailsIfPosIsNegative) {
  VariableExpander expander({{"machine_name", "chromebook"}});
  std::string str = "${machine_name,-3,10}";
  EXPECT_FALSE(expander.ExpandString(&str));
  EXPECT_EQ(str, "${machine_name,-3,10}");
}

TEST(VariableExpanderTest, FailsIfCountIsNegative) {
  VariableExpander expander({{"machine_name", "chromebook"}});
  std::string str = "${machine_name,2,-3}";
  EXPECT_FALSE(expander.ExpandString(&str));
  EXPECT_EQ(str, "${machine_name,2,-3}");
}

TEST(VariableExpanderTest, IgnoresWhitespace) {
  VariableExpander expander({{"machine_name", "chromebook"}});
  std::string str = "${machine_name , 2 ,   4 }";
  EXPECT_TRUE(expander.ExpandString(&str));
  EXPECT_EQ(str, "rome");
}

TEST(VariableExpanderTest, FailsOnInvalidRange) {
  VariableExpander expander({{"machine_name", "chromebook"}});
  std::string str = "${machine_name,2-wo,4$$our}";
  EXPECT_FALSE(expander.ExpandString(&str));
  EXPECT_EQ(str, "${machine_name,2-wo,4$$our}");
}

TEST(VariableExpanderTest, FailsIfTokenHasPostfix) {
  VariableExpander expander({{"machine_name", "chromebook"}});
  std::string str = "${machine_namePOSTFIX}";
  EXPECT_FALSE(expander.ExpandString(&str));
  EXPECT_EQ(str, "${machine_namePOSTFIX}");
}

// Trips if ReplaceSubstringsAfterOffset is used in variable expander. Found by
// the fuzzer!
TEST(VariableExpanderTest, DoesNotRecurse) {
  VariableExpander expander(
      {{"machine_name", "${machine_name}${machine_name}"}});
  std::string str = "${machine_name}${machine_name}";
  EXPECT_TRUE(expander.ExpandString(&str));
  EXPECT_EQ(str,
            "${machine_name}${machine_name}${machine_name}${machine_name}");
}

TEST(VariableExpanderTest, EdgeCases) {
  VariableExpander expander({{"machine_name", "XXX"}});
  std::string str = "$${${ma${${machine_name${machine_name}}}${machine_name";
  EXPECT_FALSE(expander.ExpandString(&str));
  EXPECT_EQ(str, "$${${ma${${machine_name${machine_name}}}${machine_name");
}

TEST(VariableExpanderTest, ExpandValueSucceeds) {
  base::Value root(base::Value::Type::DICT);
  base::Value::List list;
  list.Append(123);
  list.Append("${machine_name}");
  list.Append(true);
  root.GetDict().Set("list", std::move(list));
  root.GetDict().Set("str", "${machine_name}");
  root.GetDict().Set("double", 123.45);

  VariableExpander expander({{"machine_name", "chromebook"}});
  EXPECT_TRUE(expander.ExpandValue(&root));

  const base::Value::Dict& root_dict = root.GetDict();
  const base::Value::List& expanded_list =
      CHECK_DEREF(root_dict.FindList("list"));
  EXPECT_EQ(expanded_list[0].GetInt(), 123);
  EXPECT_EQ(expanded_list[1].GetString(), "chromebook");
  EXPECT_EQ(expanded_list[2].GetBool(), true);
  EXPECT_EQ(CHECK_DEREF(root_dict.FindString("str")), "chromebook");
  EXPECT_EQ(root_dict.FindDouble("double"), 123.45);
}

TEST(VariableExpanderTest, ExpandValueExpandsOnlyGoodVariables) {
  base::Value root(base::Value::Type::DICT);
  root.GetDict().Set("str1", "${machine_nameBAD}");
  root.GetDict().Set("str2", "${machine_name}");

  VariableExpander expander({{"machine_name", "chromebook"}});
  EXPECT_FALSE(expander.ExpandValue(&root));

  const base::Value::Dict& root_dict = root.GetDict();
  EXPECT_EQ(CHECK_DEREF(root_dict.FindString("str1")), "${machine_nameBAD}");
  EXPECT_EQ(CHECK_DEREF(root_dict.FindString("str2")), "chromebook");
}

}  // namespace chromeos::variable_expander
