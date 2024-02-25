// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/lib/persistent_proto.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
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

}  // namespace

class PersistentProtoTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

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

  void WriteToDisk(const KeyProto& proto) {
    ASSERT_TRUE(base::WriteFile(GetPath(), proto.SerializeAsString()));
  }

  void OnRead(const ReadStatus status) {
    read_status_ = status;
    ++read_count_;
  }

  base::OnceCallback<void(ReadStatus)> ReadCallback() {
    return base::BindOnce(&PersistentProtoTest::OnRead, base::Unretained(this));
  }

  void OnWrite(const WriteStatus status) {
    ASSERT_EQ(status, WriteStatus::kOk);
    ++write_count_;
  }

  base::RepeatingCallback<void(WriteStatus)> WriteCallback() {
    return base::BindRepeating(&PersistentProtoTest::OnWrite,
                               base::Unretained(this));
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  // Records the information passed to the callbacks for later expectation.
  ReadStatus read_status_;
  int read_count_ = 0;
  int write_count_ = 0;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  base::ScopedTempDir temp_dir_;
};

// Test that the underlying proto is nullptr until a read is complete, and isn't
// after that.
TEST_F(PersistentProtoTest, Initialization) {
  PersistentProto<KeyProto> pproto(GetPath(), WriteDelay(), ReadCallback(),
                                   WriteCallback());
  EXPECT_EQ(pproto.get(), nullptr);
  Wait();
  EXPECT_NE(pproto.get(), nullptr);
}

// Test bool conversion and has_value.
TEST_F(PersistentProtoTest, BoolTests) {
  PersistentProto<KeyProto> pproto(GetPath(), WriteDelay(), ReadCallback(),
                                   WriteCallback());
  EXPECT_EQ(pproto.get(), nullptr);
  EXPECT_FALSE(pproto);
  EXPECT_FALSE(pproto.has_value());
  Wait();
  EXPECT_NE(pproto.get(), nullptr);
  EXPECT_TRUE(pproto);
  EXPECT_TRUE(pproto.has_value());
}

// Test -> and *.
TEST_F(PersistentProtoTest, Getters) {
  PersistentProto<KeyProto> pproto(GetPath(), WriteDelay(), ReadCallback(),
                                   WriteCallback());
  Wait();
  // We're really just checking these don't crash.
  EXPECT_EQ(pproto->last_rotation(), 0);
  KeyProto val = *pproto;
}

// Test that the pproto correctly saves the in-memory proto to disk.
TEST_F(PersistentProtoTest, Read) {
  PersistentProto<KeyProto> pproto(GetPath(), WriteDelay(), ReadCallback(),
                                   WriteCallback());
  // Underlying proto should be nullptr until read is complete.
  EXPECT_EQ(pproto.get(), nullptr);

  Wait();
  EXPECT_EQ(read_status_, ReadStatus::kMissing);
  EXPECT_EQ(read_count_, 1);
  EXPECT_EQ(write_count_, 1);

  PopulateTestProto(pproto.get());
  pproto.StartWriteForTesting();
  Wait();
  EXPECT_EQ(write_count_, 2);

  KeyProto written = ReadFromDisk();
  EXPECT_TRUE(ProtoEquals(&written, pproto.get()));
}

// Test that invalid files on disk are handled correctly.
TEST_F(PersistentProtoTest, ReadInvalidProto) {
  ASSERT_TRUE(base::WriteFile(GetPath(), "this isn't a valid proto"));

  PersistentProto<KeyProto> pproto(GetPath(), WriteDelay(), ReadCallback(),
                                   WriteCallback());
  Wait();
  EXPECT_EQ(read_status_, ReadStatus::kParseError);
  EXPECT_EQ(read_count_, 1);
  EXPECT_EQ(write_count_, 1);
}

// Test that the pproto correctly loads an on-disk proto into memory.
TEST_F(PersistentProtoTest, Write) {
  const auto test_proto = MakeTestProto();
  WriteToDisk(test_proto);

  PersistentProto<KeyProto> pproto(GetPath(), WriteDelay(), ReadCallback(),
                                   WriteCallback());
  EXPECT_EQ(pproto.get(), nullptr);

  Wait();
  EXPECT_EQ(read_status_, ReadStatus::kOk);
  EXPECT_EQ(read_count_, 1);
  EXPECT_EQ(write_count_, 0);
  EXPECT_NE(pproto.get(), nullptr);
  EXPECT_TRUE(ProtoEquals(pproto.get(), &test_proto));
}

// Test that several saves all happen correctly.
TEST_F(PersistentProtoTest, MultipleWrites) {
  PersistentProto<KeyProto> pproto(GetPath(), WriteDelay(), ReadCallback(),
                                   WriteCallback());
  EXPECT_EQ(pproto.get(), nullptr);

  Wait();
  EXPECT_EQ(write_count_, 1);

  for (int i = 1; i <= 10; ++i) {
    pproto.get()->set_last_rotation(i * i);
    pproto.StartWriteForTesting();
    Wait();
    EXPECT_EQ(write_count_, i + 1);

    KeyProto written = ReadFromDisk();
    ASSERT_EQ(written.last_rotation(), i * i);
  }
}

// Test that many calls to QueueWrite get batched, leading to only one real
// write.
TEST_F(PersistentProtoTest, QueueWrites) {
  PersistentProto<KeyProto> pproto(GetPath(), WriteDelay(), ReadCallback(),
                                   WriteCallback());
  Wait();
  EXPECT_EQ(write_count_, 1);

  // Three successive StartWrite calls result in three writes.
  write_count_ = 0;
  for (int i = 0; i < 3; ++i) {
    pproto.StartWriteForTesting();
  }
  Wait();
  EXPECT_EQ(write_count_, 3);

  // Three successive QueueWrite calls results in one write.
  write_count_ = 0;
  for (int i = 0; i < 3; ++i) {
    pproto.QueueWrite();
  }
  Wait();
  EXPECT_EQ(write_count_, 1);
}

TEST_F(PersistentProtoTest, ClearContents) {
  const auto test_proto = MakeTestProto();
  WriteToDisk(test_proto);

  std::unique_ptr<PersistentProto<KeyProto>> pproto =
      std::make_unique<PersistentProto<KeyProto>>(
          GetPath(), WriteDelay(), ReadCallback(), WriteCallback());

  EXPECT_EQ(pproto->get(), nullptr);

  Wait();
  EXPECT_EQ(read_status_, ReadStatus::kOk);
  EXPECT_EQ(read_count_, 1);
  EXPECT_EQ(write_count_, 0);

  (*pproto)->Clear();
  pproto->QueueWrite();

  pproto.reset();

  Wait();

  int64_t size = 0;
  std::string empty_proto;
  KeyProto().SerializeToString(&empty_proto);

  ASSERT_TRUE(base::GetFileSize(GetPath(), &size));
  EXPECT_EQ(size, static_cast<int64_t>(empty_proto.size()));
}
}  // namespace metrics::structured
