// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <cstdint>
#include <string>

#include "components/reporting/resources/resource_managed_buffer.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Property;
using ::testing::StrEq;

namespace reporting {
namespace {
class ResourceManagedBufferTest : public ::testing::Test {
 protected:
  void SetUp() override {
    memory_resource_ =
        base::MakeRefCounted<ResourceManager>(1u * 1024LLu);  // 1 KiB
  }

  void TearDown() override { EXPECT_THAT(memory_resource_->GetUsed(), Eq(0u)); }

  base::test::TaskEnvironment task_environment_;

  scoped_refptr<ResourceManager> memory_resource_;
};

TEST_F(ResourceManagedBufferTest, SuccessfulAllocAndClear) {
  ResourceManagedBuffer buffer(memory_resource_);
  ASSERT_OK(buffer.Allocate(1024LLu));
  EXPECT_THAT(buffer.size(), Eq(1024LLu));
  EXPECT_THAT(buffer, Not(IsEmpty()));
  buffer.Clear();
}

TEST_F(ResourceManagedBufferTest, SuccessfulAllocAndDestruct) {
  ResourceManagedBuffer buffer(memory_resource_);
  ASSERT_OK(buffer.Allocate(1024LLu));
  EXPECT_THAT(buffer.size(), Eq(1024LLu));
  EXPECT_THAT(buffer, Not(IsEmpty()));
}

TEST_F(ResourceManagedBufferTest, FailedAlloc) {
  ResourceManagedBuffer buffer(memory_resource_);
  const auto status = buffer.Allocate(2u * 1024LLu);
  EXPECT_THAT(status,
              AllOf(Property(&Status::code, Eq(error::RESOURCE_EXHAUSTED)),
                    Property(&Status::error_message,
                             StrEq("Not enough memory for the buffer"))));
  EXPECT_THAT(buffer, IsEmpty());
}

TEST_F(ResourceManagedBufferTest, SuccessfulAllocAndFillIn) {
  ResourceManagedBuffer buffer(memory_resource_);
  ASSERT_OK(buffer.Allocate(1024LLu));
  EXPECT_THAT(buffer.size(), Eq(1024LLu));
  EXPECT_THAT(buffer, Not(IsEmpty()));
  static constexpr char kData[] = "ABCDEF 0123456789";
  for (size_t i = 0; kData[i]; ++i) {
    *buffer.at(512u + i) = kData[i];
  }
  EXPECT_THAT(std::string(buffer.at(512u), std::strlen(kData)), StrEq(kData));
}

TEST_F(ResourceManagedBufferTest, MultipleAllocations) {
  ResourceManagedBuffer buffer(memory_resource_);
  // Every time we allocate something, previous buffer is released.
  for (int i = 0; i < 64; ++i) {
    ASSERT_OK(buffer.Allocate(1024LLu));
    EXPECT_THAT(buffer.size(), Eq(1024LLu));
    EXPECT_THAT(buffer, Not(IsEmpty()));
  }
}
}  // namespace
}  // namespace reporting
