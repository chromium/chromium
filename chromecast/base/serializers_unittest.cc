// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_util.h"
#include "base/values.h"
#include "chromecast/base/scoped_temp_file.h"
#include "chromecast/base/serializers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {
const char kEmptyJsonString[] = "{}";
const char kEmptyJsonFileString[] = "{\n\n}\n";
const char kProperJsonString[] =
    "{\n"
    "   \"compound\": {\n"
    "      \"a\": 1,\n"
    "      \"b\": 2\n"
    "   },\n"
    "   \"some_String\": \"1337\",\n"
    "   \"some_int\": 42,\n"
    "   \"the_list\": [ \"val1\", \"val2\" ]\n"
    "}\n";
const char kPoorlyFormedJsonString[] = "{\"key\":";
const char kTestKey[] = "test_key";
const char kTestValue[] = "test_value";

}  // namespace

TEST(DeserializeFromJson, EmptyString) {
  std::string str;
  std::unique_ptr<base::Value> value = DeserializeFromJson(str);
  EXPECT_EQ(nullptr, value.get());
}

TEST(DeserializeFromJson, EmptyJsonObject) {
  std::string str = kEmptyJsonString;
  std::unique_ptr<base::Value> value = DeserializeFromJson(str);
  EXPECT_NE(nullptr, value.get());
}

TEST(DeserializeFromJson, ProperJsonObject) {
  std::string str = kProperJsonString;
  std::unique_ptr<base::Value> value = DeserializeFromJson(str);
  EXPECT_NE(nullptr, value.get());
}

TEST(DeserializeFromJson, PoorlyFormedJsonObject) {
  std::string str = kPoorlyFormedJsonString;
  std::unique_ptr<base::Value> value = DeserializeFromJson(str);
  EXPECT_EQ(nullptr, value.get());
}

TEST(SerializeToJson, BadValue) {
  base::Value value(std::vector<char>(12));
  base::Optional<std::string> str = SerializeToJson(value);
  EXPECT_EQ(base::nullopt, str);
}

TEST(SerializeToJson, EmptyValue) {
  base::DictionaryValue value;
  base::Optional<std::string> str = SerializeToJson(value);
  ASSERT_NE(base::nullopt, str);
  EXPECT_EQ(kEmptyJsonString, *str);
}

TEST(SerializeToJson, PopulatedValue) {
  base::DictionaryValue orig_value;
  orig_value.SetString(kTestKey, kTestValue);
  base::Optional<std::string> str = SerializeToJson(orig_value);
  ASSERT_NE(nullptr, str);

  std::unique_ptr<base::Value> new_value = DeserializeFromJson(*str);
  ASSERT_NE(nullptr, new_value.get());
  EXPECT_TRUE(new_value->Equals(&orig_value));
}

TEST(DeserializeJsonFromFile, NoFile) {
  std::unique_ptr<base::Value> value =
      DeserializeJsonFromFile(base::FilePath("/file/does/not/exist.json"));
  EXPECT_EQ(nullptr, value.get());
}

TEST(DeserializeJsonFromFile, EmptyString) {
  ScopedTempFile temp;
  EXPECT_EQ(static_cast<int>(strlen("")), temp.Write(""));
  std::unique_ptr<base::Value> value = DeserializeJsonFromFile(temp.path());
  EXPECT_EQ(nullptr, value.get());
}

TEST(DeserializeJsonFromFile, EmptyJsonObject) {
  ScopedTempFile temp;
  EXPECT_EQ(static_cast<int>(strlen(kEmptyJsonString)),
            temp.Write(kEmptyJsonString));
  std::unique_ptr<base::Value> value = DeserializeJsonFromFile(temp.path());
  EXPECT_NE(nullptr, value.get());
}

TEST(DeserializeJsonFromFile, ProperJsonObject) {
  ScopedTempFile temp;
  EXPECT_EQ(static_cast<int>(strlen(kProperJsonString)),
            temp.Write(kProperJsonString));
  std::unique_ptr<base::Value> value = DeserializeJsonFromFile(temp.path());
  EXPECT_NE(nullptr, value.get());
}

TEST(DeserializeJsonFromFile, PoorlyFormedJsonObject) {
  ScopedTempFile temp;
  EXPECT_EQ(static_cast<int>(strlen(kPoorlyFormedJsonString)),
            temp.Write(kPoorlyFormedJsonString));
  std::unique_ptr<base::Value> value = DeserializeJsonFromFile(temp.path());
  EXPECT_EQ(nullptr, value.get());
}

TEST(SerializeJsonToFile, BadValue) {
  ScopedTempFile temp;

  base::Value value(std::vector<char>(12));
  ASSERT_FALSE(SerializeJsonToFile(temp.path(), value));
  std::string str(temp.Read());
  EXPECT_TRUE(str.empty());
}

TEST(SerializeJsonToFile, EmptyValue) {
  ScopedTempFile temp;

  base::DictionaryValue value;
  ASSERT_TRUE(SerializeJsonToFile(temp.path(), value));
  std::string str(temp.Read());
  ASSERT_FALSE(str.empty());
  EXPECT_EQ(kEmptyJsonFileString, str);
}

TEST(SerializeJsonToFile, PopulatedValue) {
  ScopedTempFile temp;

  base::DictionaryValue orig_value;
  orig_value.SetString(kTestKey, kTestValue);
  ASSERT_TRUE(SerializeJsonToFile(temp.path(), orig_value));
  std::string str(temp.Read());
  ASSERT_FALSE(str.empty());

  std::unique_ptr<base::Value> new_value = DeserializeJsonFromFile(temp.path());
  ASSERT_NE(nullptr, new_value.get());
  EXPECT_TRUE(new_value->Equals(&orig_value));
}

}  // namespace chromecast
