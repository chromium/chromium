// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/registry_dict.h"

#include <string>
#include <utility>

#include "base/values.h"
#include "components/policy/core/common/schema.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

TEST(RegistryDictTest, SetAndGetValue) {
  RegistryDict test_dict;

  base::Value int_value(42);
  base::Value string_value("fortytwo");

  test_dict.SetValue("one", int_value.CreateDeepCopy());
  EXPECT_EQ(1u, test_dict.values().size());
  EXPECT_EQ(int_value, *test_dict.GetValue("one"));
  EXPECT_FALSE(test_dict.GetValue("two"));

  test_dict.SetValue("two", string_value.CreateDeepCopy());
  EXPECT_EQ(2u, test_dict.values().size());
  EXPECT_EQ(int_value, *test_dict.GetValue("one"));
  EXPECT_EQ(string_value, *test_dict.GetValue("two"));

  std::unique_ptr<base::Value> one(test_dict.RemoveValue("one"));
  EXPECT_EQ(1u, test_dict.values().size());
  EXPECT_EQ(int_value, *one);
  EXPECT_FALSE(test_dict.GetValue("one"));
  EXPECT_EQ(string_value, *test_dict.GetValue("two"));

  test_dict.ClearValues();
  EXPECT_FALSE(test_dict.GetValue("one"));
  EXPECT_FALSE(test_dict.GetValue("two"));
  EXPECT_TRUE(test_dict.values().empty());
}

TEST(RegistryDictTest, CaseInsensitiveButPreservingValueNames) {
  RegistryDict test_dict;

  base::Value int_value(42);
  base::Value string_value("fortytwo");

  test_dict.SetValue("One", int_value.CreateDeepCopy());
  EXPECT_EQ(1u, test_dict.values().size());
  EXPECT_EQ(int_value, *test_dict.GetValue("oNe"));

  auto entry = test_dict.values().begin();
  ASSERT_NE(entry, test_dict.values().end());
  EXPECT_EQ("One", entry->first);

  test_dict.SetValue("ONE", string_value.CreateDeepCopy());
  EXPECT_EQ(1u, test_dict.values().size());
  EXPECT_EQ(string_value, *test_dict.GetValue("one"));

  std::unique_ptr<base::Value> removed_value(test_dict.RemoveValue("onE"));
  EXPECT_EQ(string_value, *removed_value);
  EXPECT_TRUE(test_dict.values().empty());
}

TEST(RegistryDictTest, SetAndGetKeys) {
  RegistryDict test_dict;

  base::Value int_value(42);
  base::Value string_value("fortytwo");

  std::unique_ptr<RegistryDict> subdict(new RegistryDict());
  subdict->SetValue("one", int_value.CreateDeepCopy());
  test_dict.SetKey("two", std::move(subdict));
  EXPECT_EQ(1u, test_dict.keys().size());
  RegistryDict* actual_subdict = test_dict.GetKey("two");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(int_value, *actual_subdict->GetValue("one"));

  subdict.reset(new RegistryDict());
  subdict->SetValue("three", string_value.CreateDeepCopy());
  test_dict.SetKey("four", std::move(subdict));
  EXPECT_EQ(2u, test_dict.keys().size());
  actual_subdict = test_dict.GetKey("two");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(int_value, *actual_subdict->GetValue("one"));
  actual_subdict = test_dict.GetKey("four");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(string_value, *actual_subdict->GetValue("three"));

  test_dict.ClearKeys();
  EXPECT_FALSE(test_dict.GetKey("one"));
  EXPECT_FALSE(test_dict.GetKey("three"));
  EXPECT_TRUE(test_dict.keys().empty());
}

TEST(RegistryDictTest, CaseInsensitiveButPreservingKeyNames) {
  RegistryDict test_dict;

  base::Value int_value(42);

  test_dict.SetKey("One", std::make_unique<RegistryDict>());
  EXPECT_EQ(1u, test_dict.keys().size());
  RegistryDict* actual_subdict = test_dict.GetKey("One");
  ASSERT_TRUE(actual_subdict);
  EXPECT_TRUE(actual_subdict->values().empty());

  auto entry = test_dict.keys().begin();
  ASSERT_NE(entry, test_dict.keys().end());
  EXPECT_EQ("One", entry->first);

  std::unique_ptr<RegistryDict> subdict(new RegistryDict());
  subdict->SetValue("two", int_value.CreateDeepCopy());
  test_dict.SetKey("ONE", std::move(subdict));
  EXPECT_EQ(1u, test_dict.keys().size());
  actual_subdict = test_dict.GetKey("One");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(int_value, *actual_subdict->GetValue("two"));

  std::unique_ptr<RegistryDict> removed_key(test_dict.RemoveKey("one"));
  ASSERT_TRUE(removed_key);
  EXPECT_EQ(int_value, *removed_key->GetValue("two"));
  EXPECT_TRUE(test_dict.keys().empty());
}

TEST(RegistryDictTest, Merge) {
  RegistryDict dict_a;
  RegistryDict dict_b;

  base::Value int_value(42);
  base::Value string_value("fortytwo");

  dict_a.SetValue("one", int_value.CreateDeepCopy());
  std::unique_ptr<RegistryDict> subdict(new RegistryDict());
  subdict->SetValue("two", string_value.CreateDeepCopy());
  dict_a.SetKey("three", std::move(subdict));

  dict_b.SetValue("four", string_value.CreateDeepCopy());
  subdict.reset(new RegistryDict());
  subdict->SetValue("two", int_value.CreateDeepCopy());
  dict_b.SetKey("three", std::move(subdict));
  subdict.reset(new RegistryDict());
  subdict->SetValue("five", int_value.CreateDeepCopy());
  dict_b.SetKey("six", std::move(subdict));

  dict_a.Merge(dict_b);

  EXPECT_EQ(int_value, *dict_a.GetValue("one"));
  EXPECT_EQ(string_value, *dict_b.GetValue("four"));
  RegistryDict* actual_subdict = dict_a.GetKey("three");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(int_value, *actual_subdict->GetValue("two"));
  actual_subdict = dict_a.GetKey("six");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(int_value, *actual_subdict->GetValue("five"));
}

TEST(RegistryDictTest, Swap) {
  RegistryDict dict_a;
  RegistryDict dict_b;

  base::Value int_value(42);
  base::Value string_value("fortytwo");

  dict_a.SetValue("one", int_value.CreateDeepCopy());
  dict_a.SetKey("two", std::make_unique<RegistryDict>());
  dict_b.SetValue("three", string_value.CreateDeepCopy());

  dict_a.Swap(&dict_b);

  EXPECT_EQ(int_value, *dict_b.GetValue("one"));
  EXPECT_TRUE(dict_b.GetKey("two"));
  EXPECT_FALSE(dict_b.GetValue("two"));

  EXPECT_EQ(string_value, *dict_a.GetValue("three"));
  EXPECT_FALSE(dict_a.GetValue("one"));
  EXPECT_FALSE(dict_a.GetKey("two"));
}

#if defined(OS_WIN)
TEST(RegistryDictTest, ConvertToJSON) {
  RegistryDict test_dict;

  base::Value int_value(42);
  base::Value string_value("fortytwo");
  base::Value string_zero("0");
  base::Value string_dict("{ \"key\": [ \"value\" ] }");

  test_dict.SetValue("one", int_value.CreateDeepCopy());
  std::unique_ptr<RegistryDict> subdict(new RegistryDict());
  subdict->SetValue("two", string_value.CreateDeepCopy());
  test_dict.SetKey("three", std::move(subdict));
  std::unique_ptr<RegistryDict> list(new RegistryDict());
  list->SetValue("1", string_value.CreateDeepCopy());
  test_dict.SetKey("dict-to-list", std::move(list));
  test_dict.SetValue("int-to-bool", int_value.CreateDeepCopy());
  test_dict.SetValue("int-to-double", int_value.CreateDeepCopy());
  test_dict.SetValue("string-to-bool", string_zero.CreateDeepCopy());
  test_dict.SetValue("string-to-double", string_zero.CreateDeepCopy());
  test_dict.SetValue("string-to-int", string_zero.CreateDeepCopy());
  test_dict.SetValue("string-to-dict", string_dict.CreateDeepCopy());

  std::string error;
  Schema schema = Schema::Parse(
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"dict-to-list\": {"
      "      \"type\": \"array\","
      "      \"items\": { \"type\": \"string\" }"
      "    },"
      "    \"int-to-bool\": { \"type\": \"boolean\" },"
      "    \"int-to-double\": { \"type\": \"number\" },"
      "    \"string-to-bool\": { \"type\": \"boolean\" },"
      "    \"string-to-double\": { \"type\": \"number\" },"
      "    \"string-to-int\": { \"type\": \"integer\" },"
      "    \"string-to-dict\": { \"type\": \"object\" }"
      "  }"
      "}",
      &error);
  ASSERT_TRUE(schema.valid()) << error;

  std::unique_ptr<base::Value> actual(test_dict.ConvertToJSON(schema));

  base::DictionaryValue expected;
  expected.SetKey("one", int_value.Clone());
  auto expected_subdict = std::make_unique<base::DictionaryValue>();
  expected_subdict->SetKey("two", string_value.Clone());
  expected.Set("three", std::move(expected_subdict));
  auto expected_list = std::make_unique<base::ListValue>();
  expected_list->Append(std::make_unique<base::Value>(string_value.Clone()));
  expected.Set("dict-to-list", std::move(expected_list));
  expected.SetBoolean("int-to-bool", true);
  expected.SetDouble("int-to-double", 42.0);
  expected.SetBoolean("string-to-bool", false);
  expected.SetDouble("string-to-double", 0.0);
  expected.SetInteger("string-to-int", static_cast<int>(0));
  expected_list = std::make_unique<base::ListValue>();
  expected_list->Append(std::make_unique<base::Value>("value"));
  expected_subdict = std::make_unique<base::DictionaryValue>();
  expected_subdict->Set("key", std::move(expected_list));
  expected.Set("string-to-dict", std::move(expected_subdict));

  EXPECT_EQ(expected, *actual);
}

TEST(RegistryDictTest, NonSequentialConvertToJSON) {
  RegistryDict test_dict;

  std::unique_ptr<RegistryDict> list(new RegistryDict());
  list->SetValue("1", base::Value("1").CreateDeepCopy());
  list->SetValue("2", base::Value("2").CreateDeepCopy());
  list->SetValue("THREE", base::Value("3").CreateDeepCopy());
  list->SetValue("4", base::Value("4").CreateDeepCopy());
  test_dict.SetKey("dict-to-list", std::move(list));

  std::string error;
  Schema schema = Schema::Parse(
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"dict-to-list\": {"
      "      \"type\": \"array\","
      "      \"items\": { \"type\": \"string\" }"
      "    }"
      "  }"
      "}",
      &error);
  ASSERT_TRUE(schema.valid()) << error;

  std::unique_ptr<base::Value> actual(test_dict.ConvertToJSON(schema));

  base::DictionaryValue expected;
  std::unique_ptr<base::ListValue> expected_list(new base::ListValue());
  expected_list->Append(base::Value("1").CreateDeepCopy());
  expected_list->Append(base::Value("2").CreateDeepCopy());
  expected_list->Append(base::Value("4").CreateDeepCopy());
  expected.Set("dict-to-list", std::move(expected_list));

  EXPECT_EQ(expected, *actual);
}
#endif

TEST(RegistryDictTest, KeyValueNameClashes) {
  RegistryDict test_dict;

  base::Value int_value(42);
  base::Value string_value("fortytwo");

  test_dict.SetValue("one", int_value.CreateDeepCopy());
  std::unique_ptr<RegistryDict> subdict(new RegistryDict());
  subdict->SetValue("two", string_value.CreateDeepCopy());
  test_dict.SetKey("one", std::move(subdict));

  EXPECT_EQ(int_value, *test_dict.GetValue("one"));
  RegistryDict* actual_subdict = test_dict.GetKey("one");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(string_value, *actual_subdict->GetValue("two"));
}

}  // namespace
}  // namespace policy
