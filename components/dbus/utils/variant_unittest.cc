// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/utils/variant.h"

#include <unistd.h>

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "components/dbus/utils/read_value.h"
#include "components/dbus/utils/write_value.h"
#include "dbus/message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dbus_utils {
namespace {

TEST(DBusVariantTest, DefaultConstruction) {
  Variant v;
  EXPECT_TRUE(v.signature().empty());
}

TEST(DBusVariantTest, WrapSignature) {
  using TestTuple = std::tuple<int32_t, std::string>;
  using TestMap = std::map<uint16_t, bool>;
  using TestVector = std::vector<double>;

  EXPECT_EQ(Variant::Wrap<"b">(true).signature(), "b");
  EXPECT_EQ(Variant::Wrap<"y">(uint8_t{1}).signature(), "y");
  EXPECT_EQ(Variant::Wrap<"n">(int16_t{2}).signature(), "n");
  EXPECT_EQ(Variant::Wrap<"q">(uint16_t{3}).signature(), "q");
  EXPECT_EQ(Variant::Wrap<"i">(int32_t{4}).signature(), "i");
  EXPECT_EQ(Variant::Wrap<"u">(uint32_t{5}).signature(), "u");
  EXPECT_EQ(Variant::Wrap<"x">(int64_t{6}).signature(), "x");
  EXPECT_EQ(Variant::Wrap<"t">(uint64_t{7}).signature(), "t");
  EXPECT_EQ(Variant::Wrap<"d">(8.0).signature(), "d");
  EXPECT_EQ(Variant::Wrap<"s">(std::string("nine")).signature(), "s");
  EXPECT_EQ(Variant::Wrap<"o">(dbus::ObjectPath("/ten")).signature(), "o");
  EXPECT_EQ(Variant::Wrap<"h">(base::ScopedFD()).signature(), "h");

  // Containers
  EXPECT_EQ(Variant::Wrap<"ad">(TestVector{1.0}).signature(), "ad");
  EXPECT_EQ(Variant::Wrap<"a{qb}">(TestMap{{1, true}}).signature(), "a{qb}");
  EXPECT_EQ(Variant::Wrap<"(is)">(TestTuple{1, "s"}).signature(), "(is)");

  // Nested Variant
  auto inner_variant = Variant::Wrap<"s">(std::string("hello"));
  auto outer_variant = Variant::Wrap<"v">(std::move(inner_variant));
  EXPECT_EQ(outer_variant.signature(), "v");
}

TEST(DBusVariantTest, VariantRoundTripString) {
  const std::string kInnerString = "hello variant";

  auto original_variant = dbus_utils::Variant::Wrap<"s">(kInnerString);
  EXPECT_EQ(original_variant.signature(), "s");

  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  WriteValue(writer, original_variant);

  // Test correct type retrieval and signature on Read
  dbus::MessageReader reader(response.get());
  auto variant_read_back = ReadValue<dbus_utils::Variant>(reader);
  ASSERT_TRUE(variant_read_back) << "Failed to read variant";
  EXPECT_EQ(variant_read_back->signature(), "s");

  auto str_opt = std::move(*variant_read_back).Take<std::string>();
  ASSERT_TRUE(str_opt.has_value());
  EXPECT_EQ(*str_opt, kInnerString);
  EXPECT_TRUE(variant_read_back->signature().empty());  // Emptied after Take

  // Test type mismatch clears signature
  dbus::MessageReader reader_for_mismatch(response.get());
  auto variant_for_mismatch =
      ReadValue<dbus_utils::Variant>(reader_for_mismatch);
  ASSERT_TRUE(variant_for_mismatch)
      << "Failed to read variant for mismatch test";
  EXPECT_EQ(variant_for_mismatch->signature(), "s");
  auto int_opt = std::move(*variant_for_mismatch).Take<int32_t>();
  EXPECT_FALSE(int_opt.has_value());
  EXPECT_TRUE(variant_for_mismatch->signature().empty());
}

TEST(DBusVariantTest, VariantRoundTripInt32) {
  const int32_t kInnerInt = 42;

  auto original_variant = dbus_utils::Variant::Wrap<"i">(kInnerInt);
  EXPECT_EQ(original_variant.signature(), "i");
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  WriteValue(writer, original_variant);

  // Test correct type retrieval
  dbus::MessageReader reader(response.get());
  auto variant_read_back = ReadValue<dbus_utils::Variant>(reader);
  ASSERT_TRUE(variant_read_back) << "Failed to read variant";
  EXPECT_EQ(variant_read_back->signature(), "i");

  auto int_opt = std::move(*variant_read_back).Take<int32_t>();
  ASSERT_TRUE(int_opt.has_value());
  EXPECT_EQ(*int_opt, kInnerInt);
  EXPECT_TRUE(variant_read_back->signature().empty());
}

TEST(DBusVariantTest, VariantRoundTripTestStruct2) {
  using TestTuple = std::tuple<int32_t, std::string, std::vector<double>>;
  const TestTuple kExpectedData = {101, "complex", {1.5, 2.5, 3.5}};

  auto original_variant = Variant::Wrap<"(isad)">(kExpectedData);
  EXPECT_EQ(original_variant.signature(), "(isad)");
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  WriteValue(writer, original_variant);

  dbus::MessageReader reader(response.get());
  auto variant_read_back = ReadValue<Variant>(reader);
  ASSERT_TRUE(variant_read_back) << "Failed to read variant";
  EXPECT_EQ(variant_read_back->signature(), "(isad)");

  auto data_read = std::move(*variant_read_back).Take<TestTuple>();
  EXPECT_TRUE(variant_read_back->signature().empty());
  ASSERT_TRUE(data_read.has_value());
  EXPECT_EQ(*data_read, kExpectedData);
}

TEST(DBusVariantTest, VariantRoundTripMap) {
  const std::map<std::string, int32_t> kInnerMap = {
      {"one", 1}, {"two", 2}, {"three", 3}};

  auto original_variant = dbus_utils::Variant::Wrap<"a{si}">(kInnerMap);
  EXPECT_EQ(original_variant.signature(), "a{si}");
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  WriteValue(writer, original_variant);

  dbus::MessageReader reader(response.get());
  auto variant_read_back = ReadValue<dbus_utils::Variant>(reader);
  ASSERT_TRUE(variant_read_back) << "Failed to read variant";
  EXPECT_EQ(variant_read_back->signature(), "a{si}");

  auto map_opt =
      std::move(*variant_read_back).Take<std::map<std::string, int32_t>>();
  ASSERT_TRUE(map_opt.has_value());
  EXPECT_TRUE(variant_read_back->signature().empty());
  EXPECT_EQ(*map_opt, kInnerMap);
}

TEST(DBusVariantTest, VariantRoundTripVectorOfTuples) {
  using TestSimpleTuple = std::tuple<int32_t, std::string>;
  const std::vector<TestSimpleTuple> kInnerVec = {{1, "a"}, {2, "b"}};

  auto original_variant = dbus_utils::Variant::Wrap<"a(is)">(kInnerVec);
  EXPECT_EQ(original_variant.signature(), "a(is)");
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  WriteValue(writer, original_variant);

  dbus::MessageReader reader(response.get());
  auto variant_read_back = ReadValue<dbus_utils::Variant>(reader);
  ASSERT_TRUE(variant_read_back) << "Failed to read variant";
  EXPECT_EQ(variant_read_back->signature(), "a(is)");

  auto vec_opt =
      std::move(*variant_read_back).Take<std::vector<TestSimpleTuple>>();
  ASSERT_TRUE(vec_opt.has_value());
  EXPECT_TRUE(variant_read_back->signature().empty());
  EXPECT_EQ(*vec_opt, kInnerVec);
}

TEST(DBusVariantTest, VariantRoundTripArrayOfInts) {
  const std::vector<int32_t> kInnerArray = {1, 2, 3, 4};

  auto original_variant = dbus_utils::Variant::Wrap<"ai">(kInnerArray);
  EXPECT_EQ(original_variant.signature(), "ai");
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  WriteValue(writer, original_variant);

  dbus::MessageReader reader(response.get());
  auto variant_read_back = ReadValue<dbus_utils::Variant>(reader);
  ASSERT_TRUE(variant_read_back) << "Failed to read variant";
  EXPECT_EQ(variant_read_back->signature(), "ai");

  auto vec_opt = std::move(*variant_read_back).Take<std::vector<int32_t>>();
  ASSERT_TRUE(vec_opt.has_value());
  EXPECT_TRUE(variant_read_back->signature().empty());
  EXPECT_EQ(*vec_opt, kInnerArray);
}

TEST(DBusVariantTest, VariantOfVariantHoldingInt) {
  auto original_outer_variant = dbus_utils::Variant::Wrap<"v">(
      dbus_utils::Variant::Wrap<"i">(static_cast<int32_t>(12345)));
  EXPECT_EQ(original_outer_variant.signature(), "v");

  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  WriteValue(writer, original_outer_variant);

  dbus::MessageReader reader(response.get());
  auto outer_variant_read_back = ReadValue<dbus_utils::Variant>(reader);
  ASSERT_TRUE(outer_variant_read_back) << "Failed to read outer variant";
  EXPECT_EQ(outer_variant_read_back->signature(), "v");
  EXPECT_FALSE(reader.HasMoreData());

  auto inner_variant_opt =
      std::move(*outer_variant_read_back).Take<dbus_utils::Variant>();
  EXPECT_TRUE(outer_variant_read_back->signature().empty());
  ASSERT_TRUE(inner_variant_opt.has_value())
      << "Outer variant did not contain an inner variant.";
  EXPECT_EQ(inner_variant_opt->signature(), "i");

  // Test successful Get from inner variant.
  auto int_opt = std::move(*inner_variant_opt).Take<int32_t>();
  ASSERT_TRUE(int_opt.has_value())
      << "Inner variant did not contain an int32_t.";
  EXPECT_EQ(*int_opt, 12345);
  EXPECT_TRUE(inner_variant_opt->signature().empty());
}

TEST(DBusVariantTest, VariantGetTypeSafety) {
  const int32_t kInnerInt = 99;
  auto original_variant = dbus_utils::Variant::Wrap<"i">(kInnerInt);
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  WriteValue(writer, original_variant);

  // Read the variant and try to Get the wrong type.
  dbus::MessageReader reader1(response.get());
  auto variant1 = ReadValue<dbus_utils::Variant>(reader1);
  ASSERT_TRUE(variant1);
  auto str_opt = std::move(*variant1).Take<std::string>();
  EXPECT_FALSE(str_opt.has_value());

  // The variant is not consumed on failed Take. Read it again to Get the
  // correct type.
  dbus::MessageReader reader2(response.get());
  auto variant = ReadValue<dbus_utils::Variant>(reader2);
  ASSERT_TRUE(variant);
  EXPECT_EQ(variant->signature(), "i");
  auto int_opt = std::move(*variant).Take<int32_t>();
  ASSERT_TRUE(int_opt.has_value());
  EXPECT_EQ(*int_opt, kInnerInt);
}

TEST(DBusVariantTest, VariantIsEmptyAfterSuccessfulTake) {
  auto variant = Variant::Wrap<"s">(std::string("some string"));

  // Take the value. This consumes the variant.
  auto value = std::move(variant).Take<std::string>();
  ASSERT_TRUE(value.has_value());

  // The variant is now in a moved-from state, which the implementation defines
  // as empty.
  EXPECT_TRUE(variant.signature().empty());
}

TEST(DBusVariantTest, VariantIsEmptyAfterFailedTake) {
  auto variant = Variant::Wrap<"s">(std::string("another string"));
  ASSERT_EQ(variant.signature(), "s");

  // Attempt to take the wrong type.
  auto wrong_value = std::move(variant).Take<int32_t>();
  ASSERT_FALSE(wrong_value.has_value());

  // The variant should have been cleared.
  EXPECT_TRUE(variant.signature().empty());
}

TEST(DBusVariantTest, ConstructorConvertsTypes) {
  auto variant = Variant::Wrap<"i">(123);
  EXPECT_EQ(variant.signature(), "i");
  EXPECT_EQ(std::move(variant).Take<int32_t>(), 123);

  variant = Variant::Wrap<"n">(123);
  EXPECT_EQ(variant.signature(), "n");
  EXPECT_EQ(std::move(variant).Take<int16_t>(), 123);

  variant = Variant::Wrap<"y">(123);
  EXPECT_EQ(variant.signature(), "y");
  EXPECT_EQ(std::move(variant).Take<uint8_t>(), 123);
}

TEST(DBusVariantTest, Equality) {
  // Empty variants.
  EXPECT_EQ(Variant(), Variant());

  // Primitives.
  EXPECT_EQ(Variant::Wrap<"i">(123), Variant::Wrap<"i">(123));
  EXPECT_NE(Variant::Wrap<"i">(123), Variant::Wrap<"i">(456));

  // Different signature.
  EXPECT_NE(Variant::Wrap<"i">(123), Variant::Wrap<"u">(123));
  EXPECT_NE(Variant::Wrap<"i">(123), Variant());
  EXPECT_NE(Variant(), Variant::Wrap<"i">(123));

  EXPECT_EQ(Variant::Wrap<"s">("hello"), Variant::Wrap<"s">("hello"));
  EXPECT_NE(Variant::Wrap<"s">("hello"), Variant::Wrap<"s">("world"));

  // Containers.
  std::vector<int32_t> vec1 = {1, 2, 3};
  std::vector<int32_t> vec2 = {1, 2, 3};
  std::vector<int32_t> vec3 = {1, 2, 4};
  EXPECT_EQ(Variant::Wrap<"ai">(vec1), Variant::Wrap<"ai">(vec2));
  EXPECT_NE(Variant::Wrap<"ai">(vec1), Variant::Wrap<"ai">(vec3));

  // Nested variants.
  EXPECT_EQ(Variant::Wrap<"v">(Variant::Wrap<"i">(123)),
            Variant::Wrap<"v">(Variant::Wrap<"i">(123)));
  EXPECT_NE(Variant::Wrap<"v">(Variant::Wrap<"i">(123)),
            Variant::Wrap<"v">(Variant::Wrap<"i">(456)));

  // Moved-from variants (empty).
  Variant v1 = Variant::Wrap<"i">(123);
  Variant v2 = std::move(v1);
  EXPECT_EQ(v1, Variant());  // v1 is empty after move.
  EXPECT_EQ(v2, Variant::Wrap<"i">(123));

  // ScopedFD (comparing FDs).
  int fds1[2];
  int fds2[2];
  ASSERT_EQ(pipe(fds1), 0);
  ASSERT_EQ(pipe(fds2), 0);

  base::ScopedFD scoped_fd1_read(fds1[0]);
  base::ScopedFD scoped_fd1_write(fds1[1]);
  base::ScopedFD scoped_fd2_read(fds2[0]);
  base::ScopedFD scoped_fd2_write(fds2[1]);

  // Test inequality of different FDs.
  EXPECT_NE(Variant::Wrap<"h">(std::move(scoped_fd1_read)),
            Variant::Wrap<"h">(std::move(scoped_fd2_read)));

  // Test equality with empty FDs.
  EXPECT_EQ(Variant::Wrap<"h">(base::ScopedFD()),
            Variant::Wrap<"h">(base::ScopedFD()));
}

}  // namespace
}  // namespace dbus_utils
