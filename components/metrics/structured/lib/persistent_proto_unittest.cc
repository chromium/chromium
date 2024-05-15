// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/metrics/structured/lib/arena_persistent_proto.h"
#include "components/metrics/structured/lib/persistent_proto.h"
#include "components/metrics/structured/lib/persistent_proto_internal.h"
#include "components/metrics/structured/lib/proto/key.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics::structured {
namespace {

// Populate |proto| with some test data.
void PopulateTestProto(KeyProto* proto) {
  proto->set_key("abcdefghijkl");
  proto->set_last_rotation(12345);
  proto->set_rotation_period(54321);
}

// Make a proto with test data.
KeyProto MakeTestProto() {
  KeyProto proto;
  PopulateTestProto(&proto);
  return proto;
}

// Returns whether |actual| and |expected| are equal.
bool ProtoEquals(const KeyProto* actual, const KeyProto* expected) {
  bool equal = true;
  equal &= actual->key() == expected->key();
  equal &= actual->last_rotation() == expected->last_rotation();
  equal &= actual->rotation_period() == expected->rotation_period();
  return equal;
}

base::TimeDelta WriteDelay() {
  return base::Seconds(0);
}

template <typename T>
class TestCase {
 public:
  using PProtoType = T;

  TestCase() { Setup(); }
  TestCase(const TestCase&) = delete;
  ~TestCase() = default;

  void Setup() { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath GetPath() {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("proto"));
  }

  void ClearDisk() {
    base::DeleteFile(GetPath());
    ASSERT_FALSE(base::PathExists(GetPath()));
  }

  // Read the file at GetPath and parse it as a KeyProto.
  KeyProto ReadFromDisk() {
    std::string proto_str;
    CHECK(base::ReadFileToString(GetPath(), &proto_str));
    KeyProto proto;
    CHECK(proto.ParseFromString(proto_str));
    return proto;
  }

  void WriteToDisk(const KeyProto& proto) { WriteToDisk(GetPath(), proto); }

  void WriteToDisk(const base::FilePath& path, const KeyProto& proto) {
    ASSERT_TRUE(base::WriteFile(path, proto.SerializeAsString()));
  }

  void OnRead(const ReadStatus status) {
    read_status_ = status;
    ++read_count_;
  }

  base::OnceCallback<void(ReadStatus)> ReadCallback() {
    return base::BindOnce(&TestCase::OnRead, base::Unretained(this));
  }

  void OnWrite(const WriteStatus status) {
    ASSERT_EQ(status, WriteStatus::kOk);
    ++write_count_;
  }

  base::RepeatingCallback<void(WriteStatus)> WriteCallback() {
    return base::BindRepeating(&TestCase::OnWrite, base::Unretained(this));
  }

  // Constructs the proto of type T.
  T BuildTestProto();

  // Records the information passed to the callbacks for later expectation.
  ReadStatus read_status_;
  int read_count_ = 0;
  int write_count_ = 0;
  base::ScopedTempDir temp_dir_;
};

template <typename T>
T TestCase<T>::BuildTestProto() {
  ASSERT_TRUE(false)
      << "Invalid type parameter, please implement BuildTestProto for T";
}

template <>
PersistentProto<KeyProto>
TestCase<PersistentProto<KeyProto>>::BuildTestProto() {
  return PersistentProto<KeyProto>(GetPath(), WriteDelay(), ReadCallback(),
                                   WriteCallback());
}

template <>
ArenaPersistentProto<KeyProto>
TestCase<ArenaPersistentProto<KeyProto>>::BuildTestProto() {
  return ArenaPersistentProto<KeyProto>(GetPath(), WriteDelay(), ReadCallback(),
                                        WriteCallback());
}

}  // namespace

// Testing suite for any class that is a persistent proto. This is a series of
// tests needed by any PersistentProtoInternal implementation. Currently this
// includes: PersistentProto and ArenaPersistentProto.
template <typename T>
class PersistentProtoTest : public testing::Test {
 public:
  void Wait() { task_environment_.RunUntilIdle(); }

  T BuildTestProto() { return test_.BuildTestProto(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};

  TestCase<T> test_;
};

using Implementations =
    testing::Types<PersistentProto<KeyProto>, ArenaPersistentProto<KeyProto>>;
TYPED_TEST_SUITE(PersistentProtoTest, Implementations);

// Test that the underlying proto is nullptr until a read is complete, and isn't
// after that.
TYPED_TEST(PersistentProtoTest, Initialization) {
  auto pproto = this->BuildTestProto();

  EXPECT_EQ(pproto.get(), nullptr);
  this->Wait();
  EXPECT_NE(pproto.get(), nullptr);
}

// Test bool conversion and has_value.
TYPED_TEST(PersistentProtoTest, BoolTests) {
  auto pproto = this->BuildTestProto();
  EXPECT_EQ(pproto.get(), nullptr);
  EXPECT_FALSE(pproto);
  EXPECT_FALSE(pproto.has_value());
  this->Wait();
  EXPECT_NE(pproto.get(), nullptr);
  EXPECT_TRUE(pproto);
  EXPECT_TRUE(pproto.has_value());
}

// Test -> and *.
TYPED_TEST(PersistentProtoTest, Getters) {
  auto pproto = this->BuildTestProto();
  this->Wait();
  // We're really just checking these don't crash.
  EXPECT_EQ(pproto->last_rotation(), 0);
  KeyProto val = *pproto;
}

// Test that the pproto correctly saves the in-memory proto to disk.
TYPED_TEST(PersistentProtoTest, Read) {
  auto pproto = this->BuildTestProto();

  // Underlying proto should be nullptr until read is complete.
  EXPECT_EQ(pproto.get(), nullptr);

  this->Wait();
  EXPECT_EQ(this->test_.read_status_, ReadStatus::kMissing);
  EXPECT_EQ(this->test_.read_count_, 1);
  EXPECT_EQ(this->test_.write_count_, 1);

  PopulateTestProto(pproto.get());
  pproto.StartWriteForTesting();
  this->Wait();
  EXPECT_EQ(this->test_.write_count_, 2);

  KeyProto written = this->test_.ReadFromDisk();
  EXPECT_TRUE(ProtoEquals(&written, pproto.get()));
}

// Test that invalid files on disk are handled correctly.
TYPED_TEST(PersistentProtoTest, ReadInvalidProto) {
  ASSERT_TRUE(
      base::WriteFile(this->test_.GetPath(), "this isn't a valid proto"));

  auto pproto = this->BuildTestProto();

  this->Wait();
  EXPECT_EQ(this->test_.read_status_, ReadStatus::kParseError);
  EXPECT_EQ(this->test_.read_count_, 1);
  EXPECT_EQ(this->test_.write_count_, 1);
}

// Test that the pproto correctly loads an on-disk proto into memory.
TYPED_TEST(PersistentProtoTest, Write) {
  const auto test_proto = MakeTestProto();
  this->test_.WriteToDisk(test_proto);

  auto pproto = this->BuildTestProto();

  EXPECT_EQ(pproto.get(), nullptr);

  this->Wait();
  EXPECT_EQ(this->test_.read_status_, ReadStatus::kOk);
  EXPECT_EQ(this->test_.read_count_, 1);
  EXPECT_EQ(this->test_.write_count_, 0);
  EXPECT_NE(pproto.get(), nullptr);
  EXPECT_TRUE(ProtoEquals(pproto.get(), &test_proto));
}

// Test that several saves all happen correctly.
TYPED_TEST(PersistentProtoTest, MultipleWrites) {
  auto pproto = this->BuildTestProto();

  EXPECT_EQ(pproto.get(), nullptr);

  this->Wait();
  EXPECT_EQ(this->test_.write_count_, 1);

  for (int i = 1; i <= 10; ++i) {
    pproto.get()->set_last_rotation(i * i);
    pproto.StartWriteForTesting();
    this->Wait();
    EXPECT_EQ(this->test_.write_count_, i + 1);

    KeyProto written = this->test_.ReadFromDisk();
    ASSERT_EQ(written.last_rotation(), i * i);
  }
}

// Test that many calls to QueueWrite get batched, leading to only one real
// write.
TYPED_TEST(PersistentProtoTest, QueueWrites) {
  auto pproto = this->BuildTestProto();

  this->Wait();
  EXPECT_EQ(this->test_.write_count_, 1);

  // Three successive StartWrite calls result in three writes.
  this->test_.write_count_ = 0;
  for (int i = 0; i < 3; ++i) {
    pproto.StartWriteForTesting();
  }
  this->Wait();
  EXPECT_EQ(this->test_.write_count_, 3);

  // Three successive QueueWrite calls results in one write.
  this->test_.write_count_ = 0;
  for (int i = 0; i < 3; ++i) {
    pproto.QueueWrite();
  }
  this->Wait();
  EXPECT_EQ(this->test_.write_count_, 1);
}

TYPED_TEST(PersistentProtoTest, ClearContents) {
  const auto test_proto = MakeTestProto();
  this->test_.WriteToDisk(test_proto);

  {
    auto pproto = this->BuildTestProto();

    EXPECT_EQ(pproto.get(), nullptr);

    this->Wait();
    EXPECT_EQ(this->test_.read_status_, ReadStatus::kOk);
    EXPECT_EQ(this->test_.read_count_, 1);
    EXPECT_EQ(this->test_.write_count_, 0);

    pproto->Clear();
    pproto.QueueWrite();
  }

  this->Wait();

  int64_t size = 0;
  std::string empty_proto;
  KeyProto().SerializeToString(&empty_proto);

  ASSERT_TRUE(base::GetFileSize(this->test_.GetPath(), &size));
  EXPECT_EQ(size, static_cast<int64_t>(empty_proto.size()));
}

TYPED_TEST(PersistentProtoTest, UpdatePath) {
  const base::FilePath new_path =
      this->test_.temp_dir_.GetPath().Append(FILE_PATH_LITERAL("new_proto"));
  const int64_t kNewLastRotation = 10;

  const auto test_proto = MakeTestProto();
  this->test_.WriteToDisk(test_proto);

  auto test_proto2 = MakeTestProto();
  test_proto2.set_last_rotation(kNewLastRotation);
  this->test_.WriteToDisk(new_path, test_proto2);

  auto pproto = this->BuildTestProto();

  // Underlying proto should be nullptr until read is complete.
  EXPECT_EQ(pproto.get(), nullptr);

  this->Wait();
  EXPECT_EQ(this->test_.read_status_, ReadStatus::kOk);
  EXPECT_EQ(this->test_.read_count_, 1);
  EXPECT_EQ(this->test_.write_count_, 0);

  const KeyProto* ptr = pproto.get();

  pproto.UpdatePath(new_path, this->test_.ReadCallback(),
                    /*remove_existing=*/true);
  this->Wait();

  EXPECT_EQ(this->test_.read_status_, ReadStatus::kOk);
  EXPECT_EQ(this->test_.read_count_, 2);
  EXPECT_EQ(this->test_.write_count_, 1);

  // It is expected that the underlying proto doesn't change.
  EXPECT_EQ(ptr, pproto.get());

  // Check the content of the updated proto.
  EXPECT_EQ(ptr->key(), test_proto.key());
  EXPECT_EQ(ptr->rotation_period(), test_proto.rotation_period());
  EXPECT_EQ(ptr->last_rotation(), kNewLastRotation);

  // Check the state of the files are what we expect.
  ASSERT_FALSE(base::PathExists(this->test_.GetPath()));
  ASSERT_TRUE(base::PathExists(new_path));
}

}  // namespace metrics::structured
