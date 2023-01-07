// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/xsurface_datastore.h"

#include <map>
#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {
using testing::ElementsAre;
using testing::Eq;
using testing::Pair;
using testing::Pointee;

class MockObserver : public XsurfaceDatastoreDataReader::Observer {
 public:
  MOCK_METHOD(void,
              DatastoreEntryUpdated,
              (XsurfaceDatastoreDataReader * source, const std::string& key),
              (override));

  MOCK_METHOD(void,
              DatastoreEntryRemoved,
              (XsurfaceDatastoreDataReader * source, const std::string& key),
              (override));
};

TEST(XsurfaceDatastoreSliceTest, UpdateDatastoreEntry_StoresData) {
  XsurfaceDatastoreSlice data;
  data.UpdateDatastoreEntry("a", "b");
  data.UpdateDatastoreEntry("c", "d");

  EXPECT_THAT(data.FindEntry("a"), Pointee(Eq("b")));
  EXPECT_THAT(data.GetAllEntries(),
              ElementsAre(Pair("a", "b"), Pair("c", "d")));
}

TEST(XsurfaceDatastoreSliceTest, RemoveDatastoreEntry_RemovesData) {
  XsurfaceDatastoreSlice data;
  data.UpdateDatastoreEntry("a", "b");
  data.UpdateDatastoreEntry("c", "d");
  data.RemoveDatastoreEntry("c");

  EXPECT_THAT(data.FindEntry("c"), testing::IsNull());
  EXPECT_THAT(data.GetAllEntries(), ElementsAre(Pair("a", "b")));
}

TEST(XsurfaceDatastoreSliceTest, RemoveDatastoreEntry_NonExisting_DoesNothing) {
  XsurfaceDatastoreSlice data;
  data.RemoveDatastoreEntry("x");

  EXPECT_THAT(data.GetAllEntries(), testing::IsEmpty());
}

TEST(XsurfaceDatastoreAggregateTest, Empty_DataAccess) {
  XsurfaceDatastoreAggregate data({});
  EXPECT_THAT(data.GetAllEntries(), testing::IsEmpty());
  EXPECT_EQ(data.FindEntry("x"), nullptr);
}

TEST(XsurfaceDatastoreAggregateTest, TwoSlices_DataAccess) {
  XsurfaceDatastoreSlice one;
  one.UpdateDatastoreEntry("one/1", "1");
  XsurfaceDatastoreSlice two;
  two.UpdateDatastoreEntry("two/2", "2");
  two.UpdateDatastoreEntry("two/3", "3");
  XsurfaceDatastoreAggregate data({&one, &two});

  testing::InSequence in_sequence;
  EXPECT_THAT(
      data.GetAllEntries(),
      ElementsAre(Pair("one/1", "1"), Pair("two/2", "2"), Pair("two/3", "3")));
  EXPECT_EQ(data.FindEntry("x"), nullptr);
  EXPECT_THAT(data.FindEntry("one/1"), Pointee(Eq("1")));
  EXPECT_THAT(data.FindEntry("two/2"), Pointee(Eq("2")));
}

TEST(XsurfaceDatastoreAggregateTest, TwoSlices_EventsAreObserved) {
  XsurfaceDatastoreSlice one;
  one.UpdateDatastoreEntry("one/pre-existing", "no-events");
  XsurfaceDatastoreSlice two;
  two.UpdateDatastoreEntry("two/pre-existing", "no-events");
  XsurfaceDatastoreAggregate data({&one, &two});
  MockObserver observer;
  data.AddObserver(&observer);

  testing::InSequence in_sequence;
  EXPECT_CALL(observer, DatastoreEntryUpdated(Eq(&data), Eq("one/1"))).Times(2);
  EXPECT_CALL(observer, DatastoreEntryRemoved(Eq(&data), Eq("one/1")));
  one.UpdateDatastoreEntry("one/1", "1");
  two.UpdateDatastoreEntry("one/1", "2");
  one.RemoveDatastoreEntry("one/1");

  // Stop observing, ensure events are not sent.
  EXPECT_CALL(observer, DatastoreEntryRemoved(Eq(&data), Eq("one/1"))).Times(0);
  data.RemoveObserver(&observer);
  one.UpdateDatastoreEntry("one/1", "3");
}

}  // namespace
}  // namespace feed
