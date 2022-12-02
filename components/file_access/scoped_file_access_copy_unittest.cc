// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/file_access/scoped_file_access_copy.h"

#include <memory>

#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_access {

class ScopedFileAccessCopyTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ScopedFileAccessCopyTest, EndCallbackTest) {
  base::test::TestFuture<void> future;
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  auto file_access = std::make_unique<file_access::ScopedFileAccessCopy>(
      true, base::ScopedFD(), future.GetCallback());
#else
  auto file_access = std::make_unique<file_access::ScopedFileAccessCopy>(
      true, future.GetCallback());
#endif
  EXPECT_FALSE(future.IsReady());
  file_access.reset();
  EXPECT_TRUE(future.Wait());
}

}  // namespace file_access
