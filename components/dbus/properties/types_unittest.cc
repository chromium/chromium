// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/properties/types.h"

#include "base/memory/ref_counted_memory.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

template <typename T>
T SerializeAndDeserialize(const T& write_value) {
  auto message = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(message.get());
  write_value.Write(&writer);

  dbus::MessageReader reader(message.get());
  T read_value;
  bool success = read_value.Read(&reader);
  EXPECT_TRUE(success);
  return read_value;
}

template <typename T>
void VerifyVariantsMatch(const DbusVariant& write_value,
                         const DbusVariant& read_value) {
  const T* read_array = read_value.GetAs<T>();
  const T* write_array = write_value.GetAs<T>();
  ASSERT_TRUE(read_array);
  ASSERT_TRUE(write_array);
  EXPECT_EQ(*read_array, *write_array);
}

}  // namespace

TEST(DbusTypesTest, GetSignatureDynamic) {
  // Basic signatures.
  EXPECT_EQ("b", DbusBoolean(false).GetSignatureDynamic());
  EXPECT_EQ("i", DbusInt32(0).GetSignatureDynamic());
  EXPECT_EQ("u", DbusUint32(0).GetSignatureDynamic());
  EXPECT_EQ("s", DbusString("").GetSignatureDynamic());
  EXPECT_EQ("o", DbusObjectPath(dbus::ObjectPath("/")).GetSignatureDynamic());
  EXPECT_EQ("v", MakeDbusVariant(DbusInt32(0)).GetSignatureDynamic());
  EXPECT_EQ("ai", MakeDbusArray(DbusInt32(0)).GetSignatureDynamic());
  std::vector<unsigned char> buf{1, 2, 3};
  EXPECT_EQ(
      "ay",
      DbusByteArray(base::MakeRefCounted<base::RefCountedBytes>(std::move(buf)))
          .GetSignatureDynamic());
  EXPECT_EQ("(ib)",
            MakeDbusStruct(DbusInt32(0), DbusBoolean(0)).GetSignatureDynamic());
  EXPECT_EQ(
      "{si}",
      MakeDbusDictEntry(DbusString(""), DbusInt32(0)).GetSignatureDynamic());

  // A more complex signature.  This is the return type of
  // com.canonical.dbusmenu.GetLayout.
  EXPECT_EQ("a(ia{sv}av)",
            MakeDbusArray(MakeDbusStruct(DbusInt32(0),
                                         MakeDbusArray(MakeDbusDictEntry(
                                             DbusString(""),
                                             MakeDbusVariant(DbusInt32(0)))),
                                         DbusArray<DbusVariant>()))
                .GetSignatureDynamic());
}

TEST(DbusTypesTest, IsEqual) {
  // Types that have different signatures should never be equal.
  EXPECT_NE(DbusInt32(0), DbusBoolean(false));
  EXPECT_NE(DbusInt32(0), MakeDbusVariant(DbusInt32(0)));

  // Basic test.
  EXPECT_EQ(DbusInt32(3), DbusInt32(3));
  EXPECT_NE(DbusInt32(3), DbusInt32(4));

  // DbusVariant compares it's pointed-to value, not the pointers themselves.
  EXPECT_EQ(MakeDbusVariant(DbusInt32(3)), MakeDbusVariant(DbusInt32(3)));
  EXPECT_NE(MakeDbusVariant(DbusInt32(3)), MakeDbusVariant(DbusInt32(4)));

  // DbusByteArray does a deep comparison of its data, not a comparison on
  // pointers.
  std::vector<unsigned char> buf1{1, 2, 3};
  std::vector<unsigned char> buf2{1, 2, 3};
  EXPECT_EQ(DbusByteArray(
                base::MakeRefCounted<base::RefCountedBytes>(std::move(buf1))),
            DbusByteArray(
                base::MakeRefCounted<base::RefCountedBytes>(std::move(buf2))));
  buf1 = {1, 2, 3};
  buf2 = {3, 2, 1};
  EXPECT_NE(DbusByteArray(
                base::MakeRefCounted<base::RefCountedBytes>(std::move(buf1))),
            DbusByteArray(
                base::MakeRefCounted<base::RefCountedBytes>(std::move(buf2))));
}

TEST(DbusTypesTest, Byte) {
  DbusByte write_value(123);
  DbusByte read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);
  read_value.set_value(200);
  EXPECT_NE(write_value, read_value);
}

TEST(DbusTypesTest, Boolean) {
  DbusBoolean write_value(true);
  DbusBoolean read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);
  read_value.set_value(false);
  EXPECT_NE(write_value, read_value);
}

TEST(DbusTypesTest, Int16) {
  DbusInt16 write_value(12345);
  DbusInt16 read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);
  read_value.set_value(-5432);
  EXPECT_NE(write_value, read_value);
}

TEST(DbusTypesTest, Uint16) {
  DbusUint16 write_value(12345);
  DbusUint16 read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);
  read_value.set_value(54321);
  EXPECT_NE(write_value, read_value);
}

TEST(DbusTypesTest, Int32) {
  DbusInt32 write_value(12345);
  DbusInt32 read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);
  read_value.set_value(-54321);
  EXPECT_NE(write_value, read_value);
}

TEST(DbusTypesTest, Uint32) {
  DbusUint32 write_value(123456789);
  DbusUint32 read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);
  read_value.set_value(987654321);
  EXPECT_NE(write_value, read_value);
}

TEST(DbusTypesTest, Int64) {
  DbusInt64 write_value(123456789012345LL);
  DbusInt64 read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);
  read_value.set_value(-98765432109876LL);
  EXPECT_NE(write_value, read_value);
}

TEST(DbusTypesTest, Uint64) {
  DbusUint64 write_value(123456789012345ULL);
  DbusUint64 read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);
  read_value.set_value(987654321098765ULL);
  EXPECT_NE(write_value, read_value);
}

TEST(DbusTypesTest, Double) {
  DbusDouble write_value(3.14159);
  DbusDouble read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);
  read_value.set_value(2.71828);
  EXPECT_NE(write_value, read_value);
}

TEST(DbusTypesTest, String) {
  DbusString write_value("hello");
  DbusString read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);
  read_value.set_value("world");
  EXPECT_NE(write_value, read_value);
}

TEST(DbusTypesTest, ObjectPath) {
  DbusObjectPath write_value(dbus::ObjectPath("/example/path"));
  DbusObjectPath read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);
  read_value.set_value(dbus::ObjectPath("/different/path"));
  EXPECT_NE(write_value, read_value);
}

TEST(DbusTypesTest, VariantInt32ReadWrite) {
  DbusVariant write_value = MakeDbusVariant(DbusInt32(42));
  auto read_value = SerializeAndDeserialize(write_value);
  VerifyVariantsMatch<DbusInt32>(write_value, read_value);
}

TEST(DbusTypesTest, VariantStringReadWrite) {
  DbusVariant write_value = MakeDbusVariant(DbusString("Test variant"));
  auto read_value = SerializeAndDeserialize(write_value);
  VerifyVariantsMatch<DbusString>(write_value, read_value);
}

TEST(DbusTypesTest, ArrayInt32ReadWrite) {
  auto write_value = MakeDbusArray(DbusInt32(1), DbusInt32(2), DbusInt32(3));
  auto read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);
}

TEST(DbusTypesTest, StructReadWrite) {
  auto write_value = MakeDbusStruct(DbusInt32(42), DbusString("Hello"));
  auto read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);
}

TEST(DbusTypesTest, DictionaryReadWrite) {
  DbusDictionary write_value;
  write_value.Put("key1", MakeDbusVariant(DbusInt32(123)));
  write_value.Put("key2", MakeDbusVariant(DbusString("value")));
  auto read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);
}

TEST(DbusTypesTest, DictionaryPutAndGet) {
  DbusDictionary write_value;
  write_value.PutAs("key1", DbusInt32(123));
  write_value.PutAs("key2", DbusString("value"));
  auto read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);

  ASSERT_TRUE(read_value.GetAs<DbusInt32>("key1"));
  EXPECT_EQ(read_value.GetAs<DbusInt32>("key1")->value(), 123);

  ASSERT_TRUE(read_value.GetAs<DbusString>("key2"));
  EXPECT_EQ(read_value.GetAs<DbusString>("key2")->value(), "value");
}

TEST(DbusTypesTest, ByteArrayReadWrite) {
  std::vector<uint8_t> bytes = {0x01, 0x02, 0x03};
  DbusByteArray write_value(base::MakeRefCounted<base::RefCountedBytes>(bytes));
  auto read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);
}

TEST(DbusTypesTest, ArrayOfVariantsReadWrite) {
  auto write_value = MakeDbusArray(MakeDbusVariant(DbusInt32(10)),
                                   MakeDbusVariant(DbusString("Test")));
  auto read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);

  const DbusInt32* int_value = read_value.value()[0].GetAs<DbusInt32>();
  ASSERT_TRUE(int_value != nullptr);
  EXPECT_EQ(int_value->value(), 10);

  const DbusString* str_value = read_value.value()[1].GetAs<DbusString>();
  ASSERT_TRUE(str_value != nullptr);
  EXPECT_EQ(str_value->value(), "Test");
}

TEST(DbusTypesTest, ComplexStructureReadWrite) {
  auto write_value =
      MakeDbusStruct(DbusInt32(1),
                     MakeDbusArray(MakeDbusDictEntry(
                         DbusString("key"), MakeDbusVariant(DbusInt32(42)))),
                     DbusArray<DbusVariant>());
  auto read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);
}

TEST(DbusTypesTest, VariantArrayReadWrite) {
  DbusVariant write_value =
      MakeDbusVariant(MakeDbusArray(DbusInt32(1), DbusInt32(2), DbusInt32(3)));
  auto read_value = SerializeAndDeserialize(write_value);
  VerifyVariantsMatch<DbusArray<DbusInt32>>(write_value, read_value);
}

TEST(DbusTypesTest, VariantStructReadWrite) {
  DbusVariant write_value =
      MakeDbusVariant(MakeDbusStruct(DbusInt32(42), DbusString("Test")));
  auto read_value = SerializeAndDeserialize(write_value);
  VerifyVariantsMatch<DbusStruct<DbusInt32, DbusString>>(write_value,
                                                         read_value);
}

TEST(DbusTypesTest, VariantArrayOfDictEntryReadWrite) {
  DbusVariant write_value = MakeDbusVariant(
      MakeDbusArray(MakeDbusDictEntry(DbusString("key1"), DbusInt32(42)),
                    MakeDbusDictEntry(DbusString("key2"), DbusInt32(84))));
  auto read_value = SerializeAndDeserialize(write_value);
  VerifyVariantsMatch<DbusArray<DbusDictEntry<DbusString, DbusInt32>>>(
      write_value, read_value);
}

TEST(DbusTypesTest, VariantDictionaryOfDictionaryReadWrite) {
  DbusVariant write_value = MakeDbusVariant(MakeDbusDictionary(
      "outer_key", MakeDbusDictionary("inner_key", DbusInt32(42))));
  auto read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);

  auto* outer_dict = read_value.GetAs<DbusDictionary>();
  ASSERT_TRUE(outer_dict);

  auto* inner_dict = outer_dict->GetAs<DbusDictionary>("outer_key");
  ASSERT_TRUE(inner_dict);

  auto* int_value = inner_dict->GetAs<DbusInt32>("inner_key");
  ASSERT_TRUE(int_value);
  EXPECT_EQ(int_value->value(), 42);
}

TEST(DbusTypesTest, ParametersReadWrite) {
  auto write_value =
      MakeDbusParameters(DbusInt32(42), DbusString("Test"), DbusBoolean(true));
  auto read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);
}

TEST(DbusTypesTest, ParametersNestedReadWrite) {
  auto write_value = MakeDbusParameters(
      DbusInt32(42), MakeDbusArray(DbusString("one"), DbusString("two")),
      MakeDbusStruct(DbusBoolean(true), DbusDouble(3.14)));
  auto read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);
}

TEST(DbusTypesTest, ParametersWithVariantReadWrite) {
  auto write_value = MakeDbusParameters(
      DbusInt32(123), MakeDbusVariant(DbusString("VariantValue")),
      DbusDouble(6.28));
  auto read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);
}

TEST(DbusTypesTest, EmptyParametersReadWrite) {
  auto write_value = MakeDbusParameters();
  auto read_value = SerializeAndDeserialize(write_value);
  EXPECT_EQ(write_value, read_value);
}

TEST(DbusTypesTest, ParametersComparison) {
  auto params1 = MakeDbusParameters(DbusInt32(1), DbusString("Test"));
  auto params2 = MakeDbusParameters(DbusInt32(1), DbusString("Test"));
  auto params3 = MakeDbusParameters(DbusInt32(2), DbusString("Test"));

  EXPECT_EQ(params1, params2);
  EXPECT_NE(params1, params3);
}

TEST(DbusTypesTest, ParametersMove) {
  auto params1 = MakeDbusParameters(DbusInt32(1), DbusString("MoveTest"));
  auto params2 = std::move(params1);

  EXPECT_EQ(params2.value(),
            std::make_tuple(DbusInt32(1), DbusString("MoveTest")));
}

TEST(DbusTypesTest, ParametersGetSignature) {
  auto params = MakeDbusParameters(DbusInt32(1), DbusString("SignatureTest"),
                                   DbusBoolean(true));
  EXPECT_EQ(params.GetSignatureDynamic(), "isb");
}

TEST(DbusTypesTest, ParametersIsParameters) {
  auto params = MakeDbusParameters(DbusInt32(1), DbusString("Test"));
  EXPECT_TRUE(params.IsParameters());
}
