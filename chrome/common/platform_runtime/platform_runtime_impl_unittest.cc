// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/platform_runtime/platform_runtime_impl.h"

#include "base/files/file_path.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace platform_runtime {

class PlatformRuntimeImplTest : public testing::Test {
 protected:
  void SetUp() override {
    PlatformRuntimeImpl::GetInstance()->UpdatePlatformRuntimeLibrary(
        base::FilePath());
  }

  void TearDown() override {
    PlatformRuntimeImpl::GetInstance()->UpdatePlatformRuntimeLibrary(
        base::FilePath());
  }
};

TEST_F(PlatformRuntimeImplTest, InitialState) {
  PlatformRuntimeImpl* runtime = PlatformRuntimeImpl::GetInstance();
  EXPECT_EQ(runtime->GetLoadedLibrary(), nullptr);
}

TEST_F(PlatformRuntimeImplTest, LoadFailure) {
  PlatformRuntimeImpl* runtime = PlatformRuntimeImpl::GetInstance();
  // Try loading a non-existent library.
  runtime->UpdatePlatformRuntimeLibrary(
      base::FilePath(FILE_PATH_LITERAL("non_existent_library")));
  EXPECT_EQ(runtime->GetLoadedLibrary(), nullptr);
}

TEST_F(PlatformRuntimeImplTest, UnloadAlreadyEmpty) {
  PlatformRuntimeImpl* runtime = PlatformRuntimeImpl::GetInstance();
  // Passing empty path when already empty should be safe.
  runtime->UpdatePlatformRuntimeLibrary(base::FilePath());
  EXPECT_EQ(runtime->GetLoadedLibrary(), nullptr);
}

TEST_F(PlatformRuntimeImplTest, LoadAndUnloadSuccess) {
  PlatformRuntimeImpl* runtime = PlatformRuntimeImpl::GetInstance();

  base::FilePath exe_path;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));
#endif

  base::FilePath library_path = exe_path.AppendASCII(
      base::GetNativeLibraryName("platform_runtime_test_lib"));

  runtime->UpdatePlatformRuntimeLibrary(library_path);
  EXPECT_NE(runtime->GetLoadedLibrary(), nullptr);

  runtime->UpdatePlatformRuntimeLibrary(base::FilePath());
  EXPECT_EQ(runtime->GetLoadedLibrary(), nullptr);
}

}  // namespace platform_runtime
