// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/onc/variable_expander.h"

#include "base/files/file_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace variable_expander {

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
  base::Value root(base::Value::Type::DICTIONARY);
  base::Value list(base::Value::Type::LIST);
  list.Append(base::Value(123));
  list.Append(base::Value("${machine_name}"));
  list.Append(base::Value(true));
  root.SetKey("list", std::move(list));
  root.SetKey("str", base::Value("${machine_name}"));
  root.SetKey("double", base::Value(123.45));

  VariableExpander expander({{"machine_name", "chromebook"}});
  EXPECT_TRUE(expander.ExpandValue(&root));

  const base::Value::List& expanded_list = root.FindKey("list")->GetList();
  EXPECT_EQ(expanded_list[0].GetInt(), 123);
  EXPECT_EQ(expanded_list[1].GetString(), "chromebook");
  EXPECT_EQ(expanded_list[2].GetBool(), true);
  EXPECT_EQ(root.FindKey("str")->GetString(), "chromebook");
  EXPECT_EQ(root.FindKey("double")->GetDouble(), 123.45);
}

TEST(VariableExpanderTest, ExpandValueExpandsOnlyGoodVariables) {
  base::Value root(base::Value::Type::DICTIONARY);
  root.SetKey("str1", base::Value("${machine_nameBAD}"));
  root.SetKey("str2", base::Value("${machine_name}"));

  VariableExpander expander({{"machine_name", "chromebook"}});
  EXPECT_FALSE(expander.ExpandValue(&root));

  EXPECT_EQ(root.FindKey("str1")->GetString(), "${machine_nameBAD}");
  EXPECT_EQ(root.FindKey("str2")->GetString(), "chromebook");
}

}  // namespace variable_expander
}  // namespace chromeos
