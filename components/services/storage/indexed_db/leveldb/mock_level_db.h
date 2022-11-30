// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LEVELDB_MOCK_LEVEL_DB_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LEVELDB_MOCK_LEVEL_DB_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace leveldb {

class MockIterator : public Iterator {
 public:
  MockIterator();
  ~MockIterator() override;

  MOCK_CONST_METHOD0(Valid, bool());

  MOCK_METHOD0(SeekToFirst, void());
  MOCK_METHOD0(SeekToLast, void());
  MOCK_METHOD1(Seek, void(const Slice& target));

  MOCK_METHOD0(Next, void());
  MOCK_METHOD0(Prev, void());

  MOCK_CONST_METHOD0(key, Slice());
  MOCK_CONST_METHOD0(value, Slice());
  MOCK_CONST_METHOD0(status, Status());
};

class MockLevelDB : public DB {
 public:
  MockLevelDB();
  ~MockLevelDB() override;

  MOCK_METHOD3(Put,
               Status(const WriteOptions& options,
                      const Slice& key,
                      const Slice& value));

  MOCK_METHOD2(Delete, Status(const WriteOptions& options, const Slice& key));

  MOCK_METHOD2(Write, Status(const WriteOptions& options, WriteBatch* updates));

  MOCK_METHOD3(Get,
               Status(const ReadOptions& options,
                      const Slice& key,
                      std::string* value));

  MOCK_METHOD1(NewIterator, Iterator*(const ReadOptions& options));

  MOCK_METHOD0(GetSnapshot, const Snapshot*());

  MOCK_METHOD1(ReleaseSnapshot, void(const Snapshot*));

  MOCK_METHOD2(GetProperty, bool(const Slice& property, std::string* value));

  MOCK_METHOD3(GetApproximateSizes,
               void(const Range* range, int n, uint64_t* sizes));

  MOCK_METHOD2(CompactRange, void(const Slice* begin, const Slice* end));
};

}  // namespace leveldb

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LEVELDB_MOCK_LEVEL_DB_H_
