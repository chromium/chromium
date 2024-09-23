// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/util.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(UnzipSoleFile, Entry) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  std::string data;
  // A zip entry sent from a Java WebDriver client (v2.20) that contains a
  // file with the contents "COW\n".
  const char kBase64ZipEntry[] =
      "UEsDBBQACAAIAJpyXEAAAAAAAAAAAAAAAAAEAAAAdGVzdHP2D+"
      "cCAFBLBwi/wAzGBgAAAAQAAAA=";
  ASSERT_TRUE(base::Base64Decode(kBase64ZipEntry, &data));
  base::FilePath file;
  Status status = UnzipSoleFile(temp_dir.GetPath(), data, &file);
  ASSERT_EQ(kOk, status.code()) << status.message();
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(file, &contents));
  ASSERT_STREQ("COW\n", contents.c_str());
}

TEST(UnzipSoleFile, Archive) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  std::string data;
  // A zip archive sent from a Python WebDriver client that contains a
  // file with the contents "COW\n".
  const char kBase64ZipArchive[] =
      "UEsDBBQAAAAAAMROi0K/wAzGBAAAAAQAAAADAAAAbW9vQ09XClBLAQIUAxQAAAAAAMROi0K/"
      "wAzGBAAAAAQAAAADAAAAAAAAAAAAAACggQAAAABtb29QSwUGAAAAAAEAAQAxAAAAJQAAAAA"
      "A";
  ASSERT_TRUE(base::Base64Decode(kBase64ZipArchive, &data));
  base::FilePath file;
  Status status = UnzipSoleFile(temp_dir.GetPath(), data, &file);
  ASSERT_EQ(kOk, status.code()) << status.message();
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(file, &contents));
  ASSERT_STREQ("COW\n", contents.c_str());
}

namespace {

const std::string_view key = "key";
const int64_t max_safe_int = (1ll << 53) - 1;

void DictNoInit(base::Value::Dict* dict) {}

void DictInitNull(base::Value::Dict* dict) {
  dict->Set(key, base::Value());
}

class DictInitBool {
  bool init_value;

 public:
  explicit DictInitBool(bool v) : init_value(v) {}
  void operator()(base::Value::Dict* dict) { dict->Set(key, init_value); }
};

class DictInitInt {
  int init_value;

 public:
  explicit DictInitInt(int v) : init_value(v) {}
  void operator()(base::Value::Dict* dict) { dict->Set(key, init_value); }
};

class DictInitDouble {
  double init_value;

 public:
  explicit DictInitDouble(double v) : init_value(v) {}
  void operator()(base::Value::Dict* dict) { dict->Set(key, init_value); }
};

class DictInitString {
  std::string init_value;

 public:
  explicit DictInitString(const std::string& v) : init_value(v) {}
  void operator()(base::Value::Dict* dict) { dict->Set(key, init_value); }
};

template <typename ResultType, typename DictInitFunc>
void TestGetOptionalValue(bool (*func_to_test)(const base::Value::Dict&,
                                               std::string_view,
                                               ResultType*,
                                               bool*),
                          DictInitFunc dict_init_func,
                          const ResultType& init_result_value,
                          const ResultType& expected_result_value,
                          bool expected_return_value,
                          bool expected_has_value) {
  base::Value::Dict dict;
  dict_init_func(&dict);

  ResultType result_value = init_result_value;
  bool has_value;
  bool return_value = func_to_test(dict, key, &result_value, &has_value);
  ASSERT_EQ(return_value, expected_return_value);
  ASSERT_EQ(result_value, expected_result_value);
  if (return_value)
    ASSERT_EQ(has_value, expected_has_value);

  result_value = init_result_value;
  return_value = func_to_test(dict, key, &result_value, nullptr);
  ASSERT_EQ(return_value, expected_return_value);
  ASSERT_EQ(result_value, expected_result_value);
}

}  // namespace

TEST(GetOptionalValue, BoolNone) {
  TestGetOptionalValue<bool>(GetOptionalBool, DictNoInit, true, true, true,
                             false);
}

TEST(GetOptionalValue, IntNone) {
  TestGetOptionalValue<int>(GetOptionalInt, DictNoInit, 12345, 12345, true,
                            false);
}

TEST(GetOptionalValue, DoubleNone) {
  TestGetOptionalValue<double>(GetOptionalDouble, DictNoInit, 123.45, 123.45,
                               true, false);
}

TEST(GetOptionalValue, StringNone) {
  TestGetOptionalValue<std::string>(GetOptionalString, DictNoInit, "abcde",
                                    "abcde", true, false);
}

TEST(GetOptionalValue, DictionaryNone) {
  base::Value::Dict dv;
  const base::Value::Dict* tmp = &dv;
  TestGetOptionalValue<const base::Value::Dict*>(
      GetOptionalDictionary, DictNoInit, tmp, tmp, true, false);
}

TEST(GetOptionalValue, ListNone) {
  base::Value::List lv;
  const base::Value::List* tmp = &lv;
  TestGetOptionalValue<const base::Value::List*>(GetOptionalList, DictNoInit,
                                                 tmp, tmp, true, false);
}

TEST(GetOptionalValue, SafeIntNone) {
  TestGetOptionalValue<int64_t>(GetOptionalSafeInt, DictNoInit, 12345, 12345,
                                true, false);
}

TEST(GetOptionalValue, BoolNull) {
  TestGetOptionalValue<bool>(GetOptionalBool, DictInitNull, true, true, false,
                             false);
}

TEST(GetOptionalValue, IntNull) {
  TestGetOptionalValue<int>(GetOptionalInt, DictInitNull, 12345, 12345, false,
                            false);
}

TEST(GetOptionalValue, DoubleNull) {
  TestGetOptionalValue<double>(GetOptionalDouble, DictInitNull, 123.45, 123.45,
                               false, false);
}

TEST(GetOptionalValue, StringNull) {
  TestGetOptionalValue<std::string>(GetOptionalString, DictInitNull, "abcde",
                                    "abcde", false, false);
}

TEST(GetOptionalValue, DictionaryNull) {
  base::Value::Dict dv;
  const base::Value::Dict* tmp = &dv;
  TestGetOptionalValue<const base::Value::Dict*>(
      GetOptionalDictionary, DictInitNull, tmp, tmp, false, false);
}

TEST(GetOptionalValue, ListNull) {
  base::Value::List lv;
  const base::Value::List* tmp = &lv;
  TestGetOptionalValue<const base::Value::List*>(GetOptionalList, DictInitNull,
                                                 tmp, tmp, false, false);
}

TEST(GetOptionalValue, SafeIntNull) {
  TestGetOptionalValue<int64_t>(GetOptionalSafeInt, DictInitNull, 12345, 12345,
                                false, false);
}

TEST(GetOptionalValue, BoolWrongType) {
  TestGetOptionalValue<bool>(GetOptionalBool, DictInitString("test"), true,
                             true, false, false);
}

TEST(GetOptionalValue, IntWrongType) {
  TestGetOptionalValue<int>(GetOptionalInt, DictInitString("test"), 12345,
                            12345, false, false);
}

TEST(GetOptionalValue, DoubleWrongType) {
  TestGetOptionalValue<double>(GetOptionalDouble, DictInitString("test"),
                               123.45, 123.45, false, false);
}

TEST(GetOptionalValue, StringWrongType) {
  TestGetOptionalValue<std::string>(GetOptionalString, DictInitBool(false),
                                    "abcde", "abcde", false, false);
}

TEST(GetOptionalValue, DictionaryWrongType) {
  base::Value::Dict dv;
  const base::Value::Dict* tmp = &dv;
  TestGetOptionalValue<const base::Value::Dict*>(
      GetOptionalDictionary, DictInitString("test"), tmp, tmp, false, false);
}

TEST(GetOptionalValue, ListWrongType) {
  base::Value::List lv;
  const base::Value::List* tmp = &lv;
  TestGetOptionalValue<const base::Value::List*>(
      GetOptionalList, DictInitString("test"), tmp, tmp, false, false);
}

TEST(GetOptionalValue, SafeIntWrongType) {
  TestGetOptionalValue<int64_t>(GetOptionalSafeInt, DictInitString("test"),
                                12345, 12345, false, false);
}

TEST(GetOptionalValue, BoolNoConversion) {
  TestGetOptionalValue<bool>(GetOptionalBool, DictInitBool(false), true, false,
                             true, true);
}

TEST(GetOptionalValue, IntNoConversion) {
  TestGetOptionalValue<int>(GetOptionalInt, DictInitInt(54321), 12345, 54321,
                            true, true);
}

TEST(GetOptionalValue, DoubleNoConversion) {
  TestGetOptionalValue<double>(GetOptionalDouble, DictInitDouble(54.321),
                               123.45, 54.321, true, true);
}

TEST(GetOptionalValue, StringNoConversion) {
  TestGetOptionalValue<std::string>(GetOptionalString, DictInitString("xyz"),
                                    "abcde", "xyz", true, true);
}

TEST(GetOptionalValue, DictionaryNoConversion) {
  base::Value::Dict dv1;
  dv1.Set("dv", "1");
  base::Value::Dict dv2;
  dv2.Set("dv", "2");

  base::Value::Dict dict;
  dict.Set(key, dv1.Clone());
  const base::Value::Dict* res = &dv2;
  bool has_value;
  bool has_dict = GetOptionalDictionary(dict, key, &res, &has_value);
  ASSERT_EQ(has_value, true);
  ASSERT_EQ(has_dict, true);
  ASSERT_EQ(*res, dv1);
}

TEST(GetOptionalValue, ListNoConversion) {
  base::Value::List lv1;
  lv1.Append("1");
  base::Value::List lv2;
  lv2.Append("2");

  base::Value::List params = lv1.Clone();

  base::Value dict(base::Value::Type::DICT);
  dict.GetDict().SetByDottedPath(key, std::move(params));
  const base::Value::List* res = &lv2;
  bool has_value;
  bool has_dict = GetOptionalList(dict.GetDict(), key, &res, &has_value);
  ASSERT_EQ(has_value, true);
  ASSERT_EQ(has_dict, true);
  ASSERT_EQ(*res, lv1);
}

TEST(GetOptionalValue, SafeIntNoConversion) {
  TestGetOptionalValue<int64_t>(GetOptionalSafeInt, DictInitInt(54321), 12345,
                                54321, true, true);
}

TEST(GetOptionalValue, DoubleToInt) {
  TestGetOptionalValue<int>(GetOptionalInt, DictInitDouble(54321.0), 12345,
                            54321, true, true);
}

TEST(GetOptionalValue, DoubleToSafeInt) {
  TestGetOptionalValue<int64_t>(GetOptionalSafeInt, DictInitDouble(54321.0),
                                12345, 54321, true, true);
}

TEST(GetOptionalValue, DoubleToIntError) {
  TestGetOptionalValue<int>(GetOptionalInt, DictInitDouble(5432.1), 12345,
                            12345, false, false);
}

TEST(GetOptionalValue, DoubleToSafeIntError) {
  TestGetOptionalValue<int64_t>(GetOptionalSafeInt, DictInitDouble(5432.1),
                                12345, 12345, false, false);
}

TEST(GetOptionalValue, IntToDouble) {
  TestGetOptionalValue<double>(GetOptionalDouble, DictInitInt(54), 123.45, 54.0,
                               true, true);
}

TEST(GetOptionalValue, SafeIntMax) {
  TestGetOptionalValue<int64_t>(GetOptionalSafeInt,
                                DictInitDouble(max_safe_int), 12345,
                                max_safe_int, true, true);
}

TEST(GetOptionalValue, SafeIntMaxToIntError) {
  TestGetOptionalValue<int>(GetOptionalInt, DictInitDouble(max_safe_int), 12345,
                            12345, false, false);
}

TEST(GetOptionalValue, SafeIntTooLarge) {
  TestGetOptionalValue<int64_t>(GetOptionalSafeInt,
                                DictInitDouble(max_safe_int + 1), 12345, 12345,
                                false, false);
}

TEST(ConvertCentimeterToInch, Zero) {
  ASSERT_EQ(0, ConvertCentimeterToInch(0));
}

TEST(ConvertCentimeterToInch, PositiveDouble) {
  ASSERT_EQ(1, ConvertCentimeterToInch(2.54));
  ASSERT_EQ(0.1, ConvertCentimeterToInch(0.254));
}

TEST(ConvertCentimeterToInch, NegativeDouble) {
  ASSERT_EQ(-1, ConvertCentimeterToInch(-2.54));
  ASSERT_EQ(-0.1, ConvertCentimeterToInch(-0.254));
}
