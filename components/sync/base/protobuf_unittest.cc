// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "components/sync/protocol/test.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(SyncProtobufTest, TestUnknownFields) {
  // This tests ensures that we retain unknown fields in protocol buffers by
  // serialising UnknownFieldsTestB, which is a superset of UnknownFieldsTestA,
  // and checking we get back to the same message after parsing/serialising via
  // UnknownFieldsTestA.
  sync_pb::UnknownFieldsTestA a;
  sync_pb::UnknownFieldsTestB b;
  sync_pb::UnknownFieldsTestB b2;

  b.set_foo(true);
  b.set_bar(true);
  std::string serialized;
  ASSERT_TRUE(b.SerializeToString(&serialized));
  ASSERT_TRUE(a.ParseFromString(serialized));
  ASSERT_TRUE(a.foo());
  std::string serialized2;
  ASSERT_TRUE(a.SerializeToString(&serialized2));
  ASSERT_TRUE(b2.ParseFromString(serialized2));
  ASSERT_TRUE(b2.foo());
  ASSERT_TRUE(b2.bar());
}

}  // namespace
