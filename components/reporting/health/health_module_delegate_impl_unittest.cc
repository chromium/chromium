// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/health/health_module_delegate_impl.h"

#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrEq;

namespace reporting {
namespace {

constexpr char kBaseFileOne[] = "base";
constexpr uint32_t kDefaultCallSize = 10u;
constexpr uint32_t kRepeatedPtrFieldSizeOverhead = 2u;
constexpr uint32_t kMaxWriteCount = 10u;
constexpr uint32_t kMaxStorage =
    kMaxWriteCount * (kRepeatedPtrFieldSizeOverhead + kDefaultCallSize);
constexpr uint32_t kTinyStorage = 2u;

void CompareHealthData(std::string_view expected, ERPHealthData got) {
  EXPECT_THAT(expected, StrEq(got.SerializeAsString()));
}

class HealthModuleDelegateImplTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  HealthDataHistory AddEnqueueRecordCall() {
    HealthDataHistory history;
    EnqueueRecordCall call;
    call.set_priority(Priority::IMMEDIATE);
    *history.mutable_enqueue_record_call() = call;
    history.set_timestamp_seconds(base::Time::Now().ToTimeT());
    return history;
  }

  base::ScopedTempDir temp_dir_;
};

TEST_F(HealthModuleDelegateImplTest, TestInit) {
  ERPHealthData ref_data;
  const std::string file_name = base::StrCat({kBaseFileOne, "0"});
  auto call = AddEnqueueRecordCall();
  *ref_data.add_history() = call;
  ASSERT_TRUE(AppendLine(temp_dir_.GetPath().AppendASCII(file_name),
                         base::HexEncode(base::as_bytes(
                             base::make_span(call.SerializeAsString()))))
                  .ok());

  HealthModuleDelegateImpl delegate(temp_dir_.GetPath(), kMaxStorage,
                                    kBaseFileOne);
  ASSERT_FALSE(delegate.IsInitialized());

  delegate.Init();
  ASSERT_TRUE(delegate.IsInitialized());
  delegate.GetERPHealthData(
      base::BindOnce(CompareHealthData, ref_data.SerializeAsString()));
}

TEST_F(HealthModuleDelegateImplTest, TestWrite) {
  ERPHealthData ref_data;
  HealthModuleDelegateImpl delegate(temp_dir_.GetPath(), kMaxStorage,
                                    kBaseFileOne);
  ASSERT_FALSE(delegate.IsInitialized());

  // Can not post before initiating.
  delegate.PostHealthRecord(AddEnqueueRecordCall());
  delegate.GetERPHealthData(
      base::BindOnce(CompareHealthData, ref_data.SerializeAsString()));

  delegate.Init();
  ASSERT_TRUE(delegate.IsInitialized());

  // Fill the local storage.
  for (uint32_t i = 0; i < kMaxWriteCount; i++) {
    auto call = AddEnqueueRecordCall();
    *ref_data.add_history() = call;
    delegate.PostHealthRecord(call);
  }
  delegate.GetERPHealthData(
      base::BindOnce(CompareHealthData, ref_data.SerializeAsString()));

  // Overwrite half of the local storage.
  for (uint32_t i = 0; i < kMaxWriteCount / 2; i++) {
    auto call = AddEnqueueRecordCall();
    *ref_data.add_history() = call;
    delegate.PostHealthRecord(call);
  }
  ref_data.mutable_history()->DeleteSubrange(0, kMaxWriteCount / 2);
  delegate.GetERPHealthData(
      base::BindOnce(CompareHealthData, ref_data.SerializeAsString()));
}

TEST_F(HealthModuleDelegateImplTest, TestOversizedWrite) {
  ERPHealthData ref_data;
  HealthModuleDelegateImpl delegate(temp_dir_.GetPath(), kTinyStorage,
                                    kBaseFileOne);

  delegate.PostHealthRecord(AddEnqueueRecordCall());
  delegate.GetERPHealthData(
      base::BindOnce(CompareHealthData, ref_data.SerializeAsString()));
}
}  // namespace
}  // namespace reporting
