// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/properties/types.h"

#include "base/memory/ref_counted_memory.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  EXPECT_EQ("ay", DbusByteArray(base::RefCountedBytes::TakeVector(&buf))
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
  EXPECT_EQ(DbusByteArray(base::RefCountedBytes::TakeVector(&buf1)),
            DbusByteArray(base::RefCountedBytes::TakeVector(&buf2)));
  buf1 = {1, 2, 3};
  buf2 = {3, 2, 1};
  EXPECT_NE(DbusByteArray(base::RefCountedBytes::TakeVector(&buf1)),
            DbusByteArray(base::RefCountedBytes::TakeVector(&buf2)));
}
