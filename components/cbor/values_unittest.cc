// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cbor/values.h"

#include <string>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace cbor {

TEST(CBORValuesTest, TestNothrow) {
  static_assert(std::is_nothrow_move_constructible<Value>::value,
                "IsNothrowMoveConstructible");
  static_assert(std::is_nothrow_default_constructible<Value>::value,
                "IsNothrowDefaultConstructible");
  static_assert(std::is_nothrow_constructible<Value, std::string&&>::value,
                "IsNothrowMoveConstructibleFromString");
  static_assert(
      std::is_nothrow_constructible<Value, Value::BinaryValue&&>::value,
      "IsNothrowMoveConstructibleFromBytestring");
  static_assert(
      std::is_nothrow_constructible<Value, Value::ArrayValue&&>::value,
      "IsNothrowMoveConstructibleFromArray");
  static_assert(std::is_nothrow_move_assignable<Value>::value,
                "IsNothrowMoveAssignable");
}

// Test constructors
TEST(CBORValuesTest, ConstructUnsigned) {
  Value value(37);
  ASSERT_EQ(Value::Type::UNSIGNED, value.type());
  EXPECT_EQ(37u, value.GetInteger());
}

TEST(CBORValuesTest, ConstructNegative) {
  Value value(-1);
  ASSERT_EQ(Value::Type::NEGATIVE, value.type());
  EXPECT_EQ(-1, value.GetInteger());
}

TEST(CBORValuesTest, ConstructStringFromConstCharPtr) {
  const char* str = "foobar";
  Value value(str);
  ASSERT_EQ(Value::Type::STRING, value.type());
  EXPECT_EQ("foobar", value.GetString());
}

TEST(CBORValuesTest, ConstructStringFromStdStringConstRef) {
  std::string str = "foobar";
  Value value(str);
  ASSERT_EQ(Value::Type::STRING, value.type());
  EXPECT_EQ("foobar", value.GetString());
}

TEST(CBORValuesTest, ConstructStringFromStdStringRefRef) {
  std::string str = "foobar";
  Value value(std::move(str));
  ASSERT_EQ(Value::Type::STRING, value.type());
  EXPECT_EQ("foobar", value.GetString());
}

TEST(CBORValuesTest, ConstructBytestring) {
  Value value(Value::BinaryValue({0xF, 0x0, 0x0, 0xB, 0xA, 0x2}));
  ASSERT_EQ(Value::Type::BYTE_STRING, value.type());
  EXPECT_EQ(Value::BinaryValue({0xF, 0x0, 0x0, 0xB, 0xA, 0x2}),
            value.GetBytestring());
}

TEST(CBORValuesTest, ConstructBytestringFromString) {
  Value value(Value("hello", Value::Type::BYTE_STRING));
  ASSERT_EQ(Value::Type::BYTE_STRING, value.type());
  EXPECT_EQ(Value::BinaryValue({'h', 'e', 'l', 'l', 'o'}),
            value.GetBytestring());
  EXPECT_EQ("hello", value.GetBytestringAsString());
}

TEST(CBORValuesTest, ConstructArray) {
  Value::ArrayValue array;
  array.emplace_back(Value("foo"));
  {
    Value value(array);
    ASSERT_EQ(Value::Type::ARRAY, value.type());
    ASSERT_EQ(1u, value.GetArray().size());
    ASSERT_EQ(Value::Type::STRING, value.GetArray()[0].type());
    EXPECT_EQ("foo", value.GetArray()[0].GetString());
  }

  array.back() = Value("bar");
  {
    Value value(std::move(array));
    ASSERT_EQ(Value::Type::ARRAY, value.type());
    ASSERT_EQ(1u, value.GetArray().size());
    ASSERT_EQ(Value::Type::STRING, value.GetArray()[0].type());
    EXPECT_EQ("bar", value.GetArray()[0].GetString());
  }
}

TEST(CBORValuesTest, ConstructMap) {
  Value::MapValue map;
  const Value key_foo("foo");
  map[Value("foo")] = Value("bar");
  {
    Value value(map);
    ASSERT_EQ(Value::Type::MAP, value.type());
    ASSERT_EQ(value.GetMap().count(key_foo), 1u);
    ASSERT_EQ(Value::Type::STRING, value.GetMap().find(key_foo)->second.type());
    EXPECT_EQ("bar", value.GetMap().find(key_foo)->second.GetString());
  }

  map[Value("foo")] = Value("baz");
  {
    Value value(std::move(map));
    ASSERT_EQ(Value::Type::MAP, value.type());
    ASSERT_EQ(value.GetMap().count(key_foo), 1u);
    ASSERT_EQ(Value::Type::STRING, value.GetMap().find(key_foo)->second.type());
    EXPECT_EQ("baz", value.GetMap().find(key_foo)->second.GetString());
  }
}

TEST(CBORValuesTest, ConstructSimpleValue) {
  Value false_value(Value::SimpleValue::FALSE_VALUE);
  ASSERT_EQ(Value::Type::SIMPLE_VALUE, false_value.type());
  EXPECT_EQ(Value::SimpleValue::FALSE_VALUE, false_value.GetSimpleValue());

  Value true_value(Value::SimpleValue::TRUE_VALUE);
  ASSERT_EQ(Value::Type::SIMPLE_VALUE, true_value.type());
  EXPECT_EQ(Value::SimpleValue::TRUE_VALUE, true_value.GetSimpleValue());

  Value null_value(Value::SimpleValue::NULL_VALUE);
  ASSERT_EQ(Value::Type::SIMPLE_VALUE, null_value.type());
  EXPECT_EQ(Value::SimpleValue::NULL_VALUE, null_value.GetSimpleValue());

  Value undefined_value(Value::SimpleValue::UNDEFINED);
  ASSERT_EQ(Value::Type::SIMPLE_VALUE, undefined_value.type());
  EXPECT_EQ(Value::SimpleValue::UNDEFINED, undefined_value.GetSimpleValue());
}

TEST(CBORValuesTest, ConstructFloat) {
  Value float_value(3.1415927);
  ASSERT_EQ(Value::Type::FLOAT_VALUE, float_value.type());
  EXPECT_EQ(3.1415927, float_value.GetDouble());
}

TEST(CBORValuesTest, ConstructSimpleBooleanValue) {
  Value true_value(true);
  ASSERT_EQ(Value::Type::SIMPLE_VALUE, true_value.type());
  EXPECT_TRUE(true_value.GetBool());

  Value false_value(false);
  ASSERT_EQ(Value::Type::SIMPLE_VALUE, false_value.type());
  EXPECT_FALSE(false_value.GetBool());
}

// Test copy constructors
TEST(CBORValuesTest, CopyUnsigned) {
  Value value(74);
  Value copied_value(value.Clone());
  ASSERT_EQ(value.type(), copied_value.type());
  EXPECT_EQ(value.GetInteger(), copied_value.GetInteger());

  Value blank;

  blank = value.Clone();
  ASSERT_EQ(value.type(), blank.type());
  EXPECT_EQ(value.GetInteger(), blank.GetInteger());
}

TEST(CBORValuesTest, CopyNegativeInt) {
  Value value(-74);
  Value copied_value(value.Clone());
  ASSERT_EQ(value.type(), copied_value.type());
  EXPECT_EQ(value.GetInteger(), copied_value.GetInteger());

  Value blank;

  blank = value.Clone();
  ASSERT_EQ(value.type(), blank.type());
  EXPECT_EQ(value.GetInteger(), blank.GetInteger());
}

TEST(CBORValuesTest, CopyString) {
  Value value("foobar");
  Value copied_value(value.Clone());
  ASSERT_EQ(value.type(), copied_value.type());
  EXPECT_EQ(value.GetString(), copied_value.GetString());

  Value blank;

  blank = value.Clone();
  ASSERT_EQ(value.type(), blank.type());
  EXPECT_EQ(value.GetString(), blank.GetString());
}

TEST(CBORValuesTest, CopyBytestring) {
  Value value(Value::BinaryValue({0xF, 0x0, 0x0, 0xB, 0xA, 0x2}));
  Value copied_value(value.Clone());
  ASSERT_EQ(value.type(), copied_value.type());
  EXPECT_EQ(value.GetBytestring(), copied_value.GetBytestring());

  Value blank;

  blank = value.Clone();
  ASSERT_EQ(value.type(), blank.type());
  EXPECT_EQ(value.GetBytestring(), blank.GetBytestring());
}

TEST(CBORValuesTest, CopyArray) {
  Value::ArrayValue array;
  array.emplace_back(123);
  Value value(std::move(array));

  Value copied_value(value.Clone());
  ASSERT_EQ(1u, copied_value.GetArray().size());
  ASSERT_TRUE(copied_value.GetArray()[0].is_unsigned());
  EXPECT_EQ(value.GetArray()[0].GetInteger(),
            copied_value.GetArray()[0].GetInteger());

  Value blank;
  blank = value.Clone();
  EXPECT_EQ(1u, blank.GetArray().size());
}

TEST(CBORValuesTest, CopyMap) {
  Value::MapValue map;
  Value key_a("a");
  map[Value("a")] = Value(123);
  Value value(std::move(map));

  Value copied_value(value.Clone());
  EXPECT_EQ(1u, copied_value.GetMap().size());
  ASSERT_EQ(value.GetMap().count(key_a), 1u);
  ASSERT_EQ(copied_value.GetMap().count(key_a), 1u);
  ASSERT_TRUE(copied_value.GetMap().find(key_a)->second.is_unsigned());
  EXPECT_EQ(value.GetMap().find(key_a)->second.GetInteger(),
            copied_value.GetMap().find(key_a)->second.GetInteger());

  Value blank;
  blank = value.Clone();
  EXPECT_EQ(1u, blank.GetMap().size());
  ASSERT_EQ(blank.GetMap().count(key_a), 1u);
  ASSERT_TRUE(blank.GetMap().find(key_a)->second.is_unsigned());
  EXPECT_EQ(value.GetMap().find(key_a)->second.GetInteger(),
            blank.GetMap().find(key_a)->second.GetInteger());
}

TEST(CBORValuesTest, CopySimpleValue) {
  Value value(Value::SimpleValue::TRUE_VALUE);
  Value copied_value(value.Clone());
  EXPECT_EQ(value.type(), copied_value.type());
  EXPECT_EQ(value.GetSimpleValue(), copied_value.GetSimpleValue());

  Value blank;

  blank = value.Clone();
  EXPECT_EQ(value.type(), blank.type());
  EXPECT_EQ(value.GetSimpleValue(), blank.GetSimpleValue());
}

TEST(CBORValuesTest, CopyFloat) {
  Value value(2.2);
  Value copied_value(value.Clone());
  ASSERT_EQ(value.type(), copied_value.type());
  EXPECT_EQ(value.GetDouble(), copied_value.GetDouble());

  Value blank;

  blank = value.Clone();
  ASSERT_EQ(value.type(), blank.type());
  EXPECT_EQ(value.GetDouble(), blank.GetDouble());
}

// Test move constructors and move-assignment
TEST(CBORValuesTest, MoveUnsigned) {
  Value value(74);
  Value moved_value(std::move(value));
  EXPECT_EQ(Value::Type::UNSIGNED, moved_value.type());
  EXPECT_EQ(74u, moved_value.GetInteger());

  Value blank;

  blank = Value(654);
  EXPECT_EQ(Value::Type::UNSIGNED, blank.type());
  EXPECT_EQ(654u, blank.GetInteger());
}

TEST(CBORValuesTest, MoveNegativeInteger) {
  Value value(-74);
  Value moved_value(std::move(value));
  EXPECT_EQ(Value::Type::NEGATIVE, moved_value.type());
  EXPECT_EQ(-74, moved_value.GetInteger());

  Value blank;

  blank = Value(-654);
  EXPECT_EQ(Value::Type::NEGATIVE, blank.type());
  EXPECT_EQ(-654, blank.GetInteger());
}

TEST(CBORValuesTest, MoveString) {
  Value value("foobar");
  Value moved_value(std::move(value));
  EXPECT_EQ(Value::Type::STRING, moved_value.type());
  EXPECT_EQ("foobar", moved_value.GetString());

  Value blank;

  blank = Value("foobar");
  EXPECT_EQ(Value::Type::STRING, blank.type());
  EXPECT_EQ("foobar", blank.GetString());
}

TEST(CBORValuesTest, MoveBytestring) {
  const Value::BinaryValue bytes({0xF, 0x0, 0x0, 0xB, 0xA, 0x2});
  Value value(bytes);
  Value moved_value(std::move(value));
  EXPECT_EQ(Value::Type::BYTE_STRING, moved_value.type());
  EXPECT_EQ(bytes, moved_value.GetBytestring());

  Value blank;

  blank = Value(bytes);
  EXPECT_EQ(Value::Type::BYTE_STRING, blank.type());
  EXPECT_EQ(bytes, blank.GetBytestring());
}

TEST(CBORValuesTest, MoveConstructMap) {
  Value::MapValue map;
  const Value key_a("a");
  map[Value("a")] = Value(123);

  Value value(std::move(map));
  Value moved_value(std::move(value));
  ASSERT_EQ(Value::Type::MAP, moved_value.type());
  ASSERT_EQ(moved_value.GetMap().count(key_a), 1u);
  ASSERT_TRUE(moved_value.GetMap().find(key_a)->second.is_unsigned());
  EXPECT_EQ(123u, moved_value.GetMap().find(key_a)->second.GetInteger());
}

TEST(CBORValuesTest, MoveAssignMap) {
  Value::MapValue map;
  const Value key_a("a");
  map[Value("a")] = Value(123);

  Value blank;
  blank = Value(std::move(map));
  ASSERT_TRUE(blank.is_map());
  ASSERT_EQ(blank.GetMap().count(key_a), 1u);
  ASSERT_TRUE(blank.GetMap().find(key_a)->second.is_unsigned());
  EXPECT_EQ(123u, blank.GetMap().find(key_a)->second.GetInteger());
}

TEST(CBORValuesTest, MoveArray) {
  Value::ArrayValue array;
  array.emplace_back(123);
  Value value(array);
  Value moved_value(std::move(value));
  EXPECT_EQ(Value::Type::ARRAY, moved_value.type());
  EXPECT_EQ(123u, moved_value.GetArray().back().GetInteger());

  Value blank;
  blank = Value(std::move(array));
  EXPECT_EQ(Value::Type::ARRAY, blank.type());
  EXPECT_EQ(123u, blank.GetArray().back().GetInteger());
}

TEST(CBORValuesTest, MoveSimpleValue) {
  Value value(Value::SimpleValue::UNDEFINED);
  Value moved_value(std::move(value));
  EXPECT_EQ(Value::Type::SIMPLE_VALUE, moved_value.type());
  EXPECT_EQ(Value::SimpleValue::UNDEFINED, moved_value.GetSimpleValue());

  Value blank;

  blank = Value(Value::SimpleValue::UNDEFINED);
  EXPECT_EQ(Value::Type::SIMPLE_VALUE, blank.type());
  EXPECT_EQ(Value::SimpleValue::UNDEFINED, blank.GetSimpleValue());
}

TEST(CBORValuesTest, MoveFloat) {
  Value value(2.2);
  Value moved_value(std::move(value));
  EXPECT_EQ(Value::Type::FLOAT_VALUE, moved_value.type());
  EXPECT_EQ(2.2, moved_value.GetDouble());

  Value blank;

  blank = Value(65.4);
  EXPECT_EQ(Value::Type::FLOAT_VALUE, blank.type());
  EXPECT_EQ(65.4, blank.GetDouble());
}

TEST(CBORValuesTest, SelfSwap) {
  Value test(1);
  std::swap(test, test);
  EXPECT_EQ(test.GetInteger(), 1u);
}

}  // namespace cbor
