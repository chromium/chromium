// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <unistd.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "components/dbus/utils/read_value.h"
#include "components/dbus/utils/signature.h"
#include "components/dbus/utils/types.h"
#include "components/dbus/utils/write_value.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dbus_utils {
namespace {

using TestTuple1 = std::tuple<int32_t, std::string, std::vector<uint8_t>>;
using TestTuple2 = std::tuple<int32_t, std::string, std::vector<double>>;
using TestTupleInner = std::tuple<int32_t, std::string>;
using TestTupleOuter = std::tuple<std::string, TestTupleInner, uint64_t>;
using TestTupleWithVector = std::tuple<std::string, std::vector<int32_t>>;
using TestTupleWithMap = std::tuple<int32_t, std::map<std::string, bool>>;
using TestMapWithTupleKey = std::map<std::tuple<int32_t, std::string>, bool>;

// Helper for round-trip serialization/deserialization testing.
// Writes the given value to a D-Bus message, then reads it back
// and checks for equality.
template <typename T>
void TestRoundTrip(const T& original_value) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  WriteValue(writer, original_value);

  dbus::MessageReader reader(response.get());
  auto value_read_back = ReadValue<T>(reader);
  ASSERT_TRUE(value_read_back) << "Failed to read back type with signature: "
                               << GetDBusTypeSignature<T>();
  EXPECT_FALSE(reader.HasMoreData())
      << "Reader has more data after reading type with signature: "
      << GetDBusTypeSignature<T>();
  EXPECT_EQ(original_value, value_read_back);
}

// Helper to get a signature as a std::string.
template <typename T>
std::string GetSignature() {
  return GetDBusTypeSignature<T>();
}

TEST(DBusTypesTest, RoundTripUint8) {
  TestRoundTrip<uint8_t>(123);
}

TEST(DBusTypesTest, RoundTripBool) {
  TestRoundTrip<bool>(true);
  TestRoundTrip<bool>(false);
}

TEST(DBusTypesTest, RoundTripInt16) {
  TestRoundTrip<int16_t>(-12345);
}

TEST(DBusTypesTest, RoundTripUint16) {
  TestRoundTrip<uint16_t>(54321);
}

TEST(DBusTypesTest, RoundTripInt32) {
  TestRoundTrip<int32_t>(-123456789);
}

TEST(DBusTypesTest, RoundTripUint32) {
  TestRoundTrip<uint32_t>(3234567890u);
}

TEST(DBusTypesTest, RoundTripInt64) {
  TestRoundTrip<int64_t>(-123456789012345LL);
}

TEST(DBusTypesTest, RoundTripUint64) {
  TestRoundTrip<uint64_t>(9876543210987654321ULL);
}

TEST(DBusTypesTest, RoundTripDouble) {
  TestRoundTrip<double>(3.1415926535);
}

TEST(DBusTypesTest, RoundTripString) {
  TestRoundTrip<std::string>("Hello, D-Bus types!");
  TestRoundTrip<std::string>("");
}

TEST(DBusTypesTest, RoundTripObjectPath) {
  TestRoundTrip<dbus::ObjectPath>(dbus::ObjectPath("/org/chromium/TestObject"));
  TestRoundTrip<dbus::ObjectPath>(dbus::ObjectPath("/"));
}

TEST(DBusTypesTest, RoundTripVectorOfInts) {
  TestRoundTrip<std::vector<int32_t>>({10, 20, -30, 0, 42});
}

TEST(DBusTypesTest, RoundTripVectorOfStrings) {
  TestRoundTrip<std::vector<std::string>>({"alpha", "beta", "", "gamma"});
}

TEST(DBusTypesTest, RoundTripVectorOfBools) {
  // std::vector<bool>'s implementation is odd, so ensure it specifically.
  TestRoundTrip<std::vector<bool>>({true, false, true, true, false});
}

TEST(DBusTypesTest, RoundTripEmptyVector) {
  TestRoundTrip<std::vector<std::string>>({});
  TestRoundTrip<std::vector<int32_t>>({});
}

TEST(DBusTypesTest, RoundTripVectorOfEmptyString) {
  TestRoundTrip<std::vector<std::string>>({"", "", ""});
}

TEST(DBusTypesTest, RoundTripVectorVectorInts) {
  TestRoundTrip<std::vector<std::vector<int32_t>>>(
      {{1, 2}, {}, {3, 4, 5}, {-1}});
}

TEST(DBusTypesTest, RoundTripVectorOfVectorsOfStrings) {
  TestRoundTrip<std::vector<std::vector<std::string>>>(
      {{"a", "b"}, {}, {"c"}, {"d", "e", "f"}});
}

TEST(DBusTypesTest, RoundTripMapStringInt) {
  TestRoundTrip<std::map<std::string, int32_t>>(
      {{"one", 1}, {"two", 2}, {"three", 3}});
}

TEST(DBusTypesTest, RoundTripEmptyMap) {
  TestRoundTrip<std::map<std::string, int32_t>>({});
  TestRoundTrip<std::map<int32_t, double>>({});
}

TEST(DBusTypesTest, RoundTripMapStringVectorInt) {
  TestRoundTrip<std::map<std::string, std::vector<int32_t>>>(
      {{"a", {1, 2}}, {"b", {}}, {"c", {3, 4, 5}}});
}

TEST(DBusTypesTest, RoundTripMapStringMapStringInt) {
  using InnerMapType = std::map<std::string, int32_t>;
  using OuterMapType = std::map<std::string, InnerMapType>;

  TestRoundTrip<OuterMapType>(
      {{"user1", {{"score", 100}, {"level", 5}}},
       {"user2", {{"score", 150}, {"level", 7}, {"rank", 1}}},
       {"user3", {}},
       {"user4", {{"items", 42}}}});
  TestRoundTrip<OuterMapType>({});
  TestRoundTrip<OuterMapType>({{"empty_inner_test", {}}});
}

TEST(DBusTypesTest, RoundTripTestStruct1) {
  TestRoundTrip<TestTuple1>({123, "TestStruct", {0xDE, 0xAD, 0xBE, 0xEF}});
  TestRoundTrip<TestTuple1>({-1, "", {}});
}

TEST(DBusTypesTest, RoundTripTestStruct2) {
  TestRoundTrip<TestTuple2>({101, "complex data", {1.5, 2.5, -3.5}});
  TestRoundTrip<TestTuple2>({0, "", {}});
}

TEST(DBusTypesTest, RoundTripVectorOfStructs) {
  TestRoundTrip<std::vector<TestTuple1>>(
      {{1, "one", {1}}, {2, "two", {2, 2}}, {3, "three", {}}});
}

TEST(DBusTypesTest, RoundTripMapStringToStruct) {
  TestRoundTrip<std::map<std::string, TestTuple1>>(
      {{"first", {1, "one", {1}}}, {"second", {2, "two", {}}}});
}

TEST(DBusTypesTest, RoundTripStructOfStruct) {
  TestRoundTrip<TestTupleOuter>(
      {"OuterLayer1", {101, "InnerPayload1"}, 1234567890ULL});
  TestRoundTrip<TestTupleOuter>({"OuterEmptyInner", {0, ""}, 0ULL});
}

TEST(DBusTypesTest, RoundTripVectorOfStructOfStruct) {
  TestRoundTrip<std::vector<TestTupleOuter>>(
      {{"Outer1", {1, "InnerA"}, 1000ULL},
       {"Outer2", {2, "InnerB"}, 2000ULL},
       {"Outer3", {3, ""}, 3000ULL}});
  TestRoundTrip<std::vector<TestTupleOuter>>({});
}

TEST(DBusTypesTest, RoundTripTupleWithVector) {
  TestRoundTrip<TestTupleWithVector>({"hello", {1, 2, 3}});
  TestRoundTrip<TestTupleWithVector>({"world", {}});
}

TEST(DBusTypesTest, RoundTripTupleWithMap) {
  TestRoundTrip<TestTupleWithMap>({42, {{"a", true}, {"b", false}}});
  TestRoundTrip<TestTupleWithMap>({-1, {}});
}

TEST(DBusTypesTest, RoundTripMapWithTupleKey) {
  TestRoundTrip<TestMapWithTupleKey>({{{1, "a"}, true}, {{2, "b"}, false}});
  TestRoundTrip<TestMapWithTupleKey>({});
}

TEST(DBusTypesTest, ReadStructNotEnoughDataInDBusMessage) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter struct_writer(nullptr);
  writer.OpenStruct(&struct_writer);
  struct_writer.AppendInt32(123);
  writer.CloseContainer(&struct_writer);

  dbus::MessageReader reader(response.get());
  EXPECT_FALSE(ReadValue<TestTuple1>(reader));
}

TEST(DBusTypesTest, ReadStructTooMuchDataInDBusMessage) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter struct_writer(nullptr);
  writer.OpenStruct(&struct_writer);
  struct_writer.AppendInt32(123);
  struct_writer.AppendString("Test");
  dbus::MessageWriter array_writer(nullptr);
  struct_writer.OpenArray("y", &array_writer);
  array_writer.AppendByte(0x01);
  array_writer.AppendByte(0x02);
  struct_writer.CloseContainer(&array_writer);
  // Add extra data not in TestStruct1
  struct_writer.AppendDouble(3.14);
  writer.CloseContainer(&struct_writer);

  dbus::MessageReader reader(response.get());
  EXPECT_FALSE(ReadValue<TestTuple1>(reader));
}

TEST(DBusTypesTest, WriteReadMultipleTypesSequentially) {
  std::unique_ptr<dbus::MethodCall> call =
      std::make_unique<dbus::MethodCall>("dummy.interface", "DummyMethod");
  dbus::MessageWriter writer(call.get());

  const std::string kArgString = "ArgumentValue";
  const uint32_t kArgUint = 99;
  const bool kArgBool = true;
  const TestTuple1 kArgStruct = {1, "s", {1, 2}};

  WriteValue(writer, kArgString);
  WriteValue(writer, kArgUint);
  WriteValue(writer, kArgBool);
  WriteValue(writer, kArgStruct);

  dbus::MessageReader reader(call.get());
  EXPECT_EQ(ReadValue<std::string>(reader), kArgString);
  EXPECT_EQ(ReadValue<uint32_t>(reader), kArgUint);
  EXPECT_EQ(ReadValue<bool>(reader), kArgBool);
  EXPECT_EQ(ReadValue<TestTuple1>(reader), kArgStruct);
}

TEST(DBusTypesTest, ReadEmptyMessageFails) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();

  dbus::MessageReader reader(response.get());
  EXPECT_FALSE(reader.HasMoreData());

  EXPECT_FALSE(ReadValue<int32_t>(reader));
  EXPECT_FALSE(ReadValue<std::string>(reader));
  EXPECT_FALSE(ReadValue<TestTuple1>(reader));
}

TEST(DBusTypesTest, RoundTripScopedFdReadablePipe) {
  int fds[2];
  ASSERT_EQ(pipe(fds), 0);
  base::ScopedFD read_fd(fds[0]);
  base::ScopedFD write_fd(fds[1]);

  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  WriteValue(writer, read_fd);

  dbus::MessageReader reader(response.get());
  auto read_fd_back = ReadValue<base::ScopedFD>(reader);
  ASSERT_TRUE(read_fd_back);
  EXPECT_TRUE(read_fd_back->is_valid());

  const char kPayload = 'X';
  ASSERT_EQ(write(write_fd.get(), &kPayload, 1), 1);
  char buf = '\0';
  ASSERT_EQ(read(read_fd_back->get(), &buf, 1), 1);
  EXPECT_EQ(buf, kPayload);
}

TEST(DBusTypesTest, RoundTripVectorOfScopedFDs) {
  constexpr int kFdCount = 3;
  std::vector<base::ScopedFD> original_fds;
  for (int i = 0; i < kFdCount; ++i) {
    int fd = open("/dev/null", O_RDONLY);
    ASSERT_GT(fd, -1);
    original_fds.emplace_back(fd);
  }

  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  WriteValue(writer, original_fds);

  dbus::MessageReader reader(response.get());
  auto fds_read_back = ReadValue<std::vector<base::ScopedFD>>(reader);
  ASSERT_TRUE(fds_read_back);

  EXPECT_EQ(fds_read_back->size(), original_fds.size());
  for (auto& fd : *fds_read_back) {
    EXPECT_TRUE(fd.is_valid());
    EXPECT_NE(lseek(fd.get(), 0, SEEK_CUR), -1);
  }
}

}  // namespace
}  // namespace dbus_utils
