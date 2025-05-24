// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/persistent_system_profile.h"

#include <memory>

#include "base/check_op.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/rand_util.h"
#include "components/variations/hashing.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

class PersistentSystemProfileTest : public testing::Test {
 public:
  const int32_t kAllocatorMemorySize = 1 << 20;  // 1 MiB

  PersistentSystemProfileTest() = default;

  PersistentSystemProfileTest(const PersistentSystemProfileTest&) = delete;
  PersistentSystemProfileTest& operator=(const PersistentSystemProfileTest&) =
      delete;

  ~PersistentSystemProfileTest() override = default;

  void SetUp() override {
    memory_allocator_ = std::make_unique<base::LocalPersistentMemoryAllocator>(
        kAllocatorMemorySize, 0, "");
    records_ = std::make_unique<PersistentSystemProfile::RecordAllocator>(
        memory_allocator_.get());
    persistent_profile_.RegisterPersistentAllocator(memory_allocator_.get());
  }

  void TearDown() override {
    persistent_profile_.DeregisterPersistentAllocator(memory_allocator_.get());
    records_.reset();
    memory_allocator_.reset();
  }

  void WriteRecord(uint8_t type, std::string_view record) {
    persistent_profile_.allocators_[0].Write(
        static_cast<PersistentSystemProfile::RecordType>(type), record);
  }

  bool ReadRecord(uint8_t* type, std::string* record) {
    PersistentSystemProfile::RecordType rec_type;

    bool success = records_->Read(&rec_type, record);
    *type = rec_type;  // Convert to uint8_t for testing.
    return success;
  }

  base::PersistentMemoryAllocator* memory_allocator() {
    return memory_allocator_.get();
  }

  PersistentSystemProfile* persistent_profile() { return &persistent_profile_; }

 private:
  PersistentSystemProfile persistent_profile_;
  std::unique_ptr<base::PersistentMemoryAllocator> memory_allocator_;
  std::unique_ptr<PersistentSystemProfile::RecordAllocator> records_;
};

TEST_F(PersistentSystemProfileTest, Create) {
  uint32_t type;
  base::PersistentMemoryAllocator::Iterator iter(memory_allocator());
  base::PersistentMemoryAllocator::Reference ref = iter.GetNext(&type);
  DCHECK(ref);
  DCHECK_NE(0U, type);
}

TEST_F(PersistentSystemProfileTest, RecordSplitting) {
  const size_t kRecordSize = 100 << 10;  // 100 KiB
  std::string buffer(kRecordSize, '\0');
  base::RandBytes(base::as_writable_byte_span(buffer));

  WriteRecord(42, buffer);

  uint8_t type;
  std::string record;
  ASSERT_TRUE(ReadRecord(&type, &record));
  EXPECT_EQ(42U, type);
  EXPECT_EQ(buffer, record);
}

TEST_F(PersistentSystemProfileTest, ProfileStorage) {
  SystemProfileProto proto1;
  SystemProfileProto::FieldTrial* trial = proto1.add_field_trial();
  trial->set_name_id(123);
  trial->set_group_id(456);

  persistent_profile()->SetSystemProfile(proto1, false);

  SystemProfileProto proto2;
  ASSERT_TRUE(PersistentSystemProfile::HasSystemProfile(*memory_allocator()));
  ASSERT_TRUE(
      PersistentSystemProfile::GetSystemProfile(*memory_allocator(), &proto2));
  ASSERT_EQ(1, proto2.field_trial_size());
  EXPECT_EQ(123U, proto2.field_trial(0).name_id());
  EXPECT_EQ(456U, proto2.field_trial(0).group_id());

  // Check that the profile can be overwritten by another incomplete profile.

  trial = proto1.add_field_trial();
  trial->set_name_id(34);
  trial->set_group_id(50);

  persistent_profile()->SetSystemProfile(proto1, false);

  ASSERT_TRUE(
      PersistentSystemProfile::GetSystemProfile(*memory_allocator(), &proto2));
  ASSERT_EQ(2, proto2.field_trial_size());
  EXPECT_EQ(123U, proto2.field_trial(0).name_id());
  EXPECT_EQ(456U, proto2.field_trial(0).group_id());
  EXPECT_EQ(34U, proto2.field_trial(1).name_id());
  EXPECT_EQ(50U, proto2.field_trial(1).group_id());

  // Check that the profile can be overwritten by a complete profile.

  trial = proto1.add_field_trial();
  trial->set_name_id(78);
  trial->set_group_id(90);

  persistent_profile()->SetSystemProfile(proto1, true);

  ASSERT_TRUE(
      PersistentSystemProfile::GetSystemProfile(*memory_allocator(), &proto2));
  ASSERT_EQ(3, proto2.field_trial_size());
  EXPECT_EQ(123U, proto2.field_trial(0).name_id());
  EXPECT_EQ(456U, proto2.field_trial(0).group_id());
  EXPECT_EQ(34U, proto2.field_trial(1).name_id());
  EXPECT_EQ(50U, proto2.field_trial(1).group_id());
  EXPECT_EQ(78U, proto2.field_trial(2).name_id());
  EXPECT_EQ(90U, proto2.field_trial(2).group_id());

  // Check that the profile won't be overwritten by a new non-complete profile.

  trial = proto1.add_field_trial();
  trial->set_name_id(0xC0DE);
  trial->set_group_id(0xFEED);

  persistent_profile()->SetSystemProfile(proto1, false);

  ASSERT_TRUE(
      PersistentSystemProfile::GetSystemProfile(*memory_allocator(), &proto2));
  ASSERT_EQ(3, proto2.field_trial_size());
  EXPECT_EQ(123U, proto2.field_trial(0).name_id());
  EXPECT_EQ(456U, proto2.field_trial(0).group_id());
  EXPECT_EQ(34U, proto2.field_trial(1).name_id());
  EXPECT_EQ(50U, proto2.field_trial(1).group_id());
  EXPECT_EQ(78U, proto2.field_trial(2).name_id());
  EXPECT_EQ(90U, proto2.field_trial(2).group_id());
}

TEST_F(PersistentSystemProfileTest, ProfileExtensions) {
  persistent_profile()->AddFieldTrial("sna", "foo");

  SystemProfileProto fetched;
  ASSERT_FALSE(
      PersistentSystemProfile::GetSystemProfile(*memory_allocator(), &fetched));

  SystemProfileProto proto;
  SystemProfileProto::FieldTrial* trial = proto.add_field_trial();
  trial->set_name_id(123);
  trial->set_group_id(456);

  // The system profile should now start fresh. In practice, field trials should
  // already be properly updated in subsequent system profiles.
  persistent_profile()->SetSystemProfile(proto, false);
  ASSERT_TRUE(
      PersistentSystemProfile::GetSystemProfile(*memory_allocator(), &fetched));
  ASSERT_EQ(1, fetched.field_trial_size());
  EXPECT_EQ(123U, fetched.field_trial(0).name_id());
  EXPECT_EQ(456U, fetched.field_trial(0).group_id());

  persistent_profile()->AddFieldTrial("foo", "bar");
  ASSERT_TRUE(
      PersistentSystemProfile::GetSystemProfile(*memory_allocator(), &fetched));
  ASSERT_EQ(2, fetched.field_trial_size());
  EXPECT_EQ(123U, fetched.field_trial(0).name_id());
  EXPECT_EQ(456U, fetched.field_trial(0).group_id());
  EXPECT_EQ(variations::HashName("foo"), fetched.field_trial(1).name_id());
  EXPECT_EQ(variations::HashName("bar"), fetched.field_trial(1).group_id());
}

TEST_F(PersistentSystemProfileTest, OverwriteFieldTrialsInProfile) {
  // Set system profile with the field trial.
  SystemProfileProto proto;
  SystemProfileProto::FieldTrial* trial = proto.add_field_trial();
  trial->set_name_id(variations::HashName("foo"));
  trial->set_group_id(456);
  persistent_profile()->SetSystemProfile(proto, false);

  // Overwrite the same trial with different group.
  persistent_profile()->AddFieldTrial("foo", "bar");

  // The fetched profile should have the latest group name,
  SystemProfileProto fetched;
  ASSERT_TRUE(
      PersistentSystemProfile::GetSystemProfile(*memory_allocator(), &fetched));
  ASSERT_EQ(1, fetched.field_trial_size());
  EXPECT_EQ(variations::HashName("foo"), fetched.field_trial(0).name_id());
  EXPECT_EQ(variations::HashName("bar"), fetched.field_trial(0).group_id());
}

TEST_F(PersistentSystemProfileTest, OverwriteFieldTrials) {
  // Set up a non-empty system profile.
  SystemProfileProto proto;
  proto.set_client_uuid("id");
  persistent_profile()->SetSystemProfile(proto, false);

  // Set and overwrite the same trial with different group.
  persistent_profile()->AddFieldTrial("foo", "bar");
  persistent_profile()->AddFieldTrial("foo", "bar2");

  // The fetched profile should have the latest group name,
  SystemProfileProto fetched;
  ASSERT_TRUE(
      PersistentSystemProfile::GetSystemProfile(*memory_allocator(), &fetched));
  ASSERT_EQ(1, fetched.field_trial_size());
  EXPECT_EQ(variations::HashName("foo"), fetched.field_trial(0).name_id());
  EXPECT_EQ(variations::HashName("bar2"), fetched.field_trial(0).group_id());
}

TEST_F(PersistentSystemProfileTest, DeleteFieldTrials) {
  // Set up a non-empty system profile.
  SystemProfileProto proto;
  proto.set_client_uuid("id");
  persistent_profile()->SetSystemProfile(proto, false);

  // Set and delete the trial.
  persistent_profile()->AddFieldTrial("foo", "bar");
  persistent_profile()->RemoveFieldTrial("foo");

  // The fetched profile should not have the deleted trial.
  SystemProfileProto fetched;
  ASSERT_TRUE(
      PersistentSystemProfile::GetSystemProfile(*memory_allocator(), &fetched));
  ASSERT_EQ(0, fetched.field_trial_size());

  // Reset the trial and the fetched profile should have the latest group name.
  persistent_profile()->AddFieldTrial("foo", "bar2");
  ASSERT_TRUE(
      PersistentSystemProfile::GetSystemProfile(*memory_allocator(), &fetched));
  ASSERT_EQ(1, fetched.field_trial_size());
  EXPECT_EQ(variations::HashName("foo"), fetched.field_trial(0).name_id());
  EXPECT_EQ(variations::HashName("bar2"), fetched.field_trial(0).group_id());
}

}  // namespace metrics
