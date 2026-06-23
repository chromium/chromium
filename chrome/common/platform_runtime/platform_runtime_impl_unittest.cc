// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/platform_runtime/platform_runtime_impl.h"

#include <algorithm>
#include <optional>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace platform_runtime {

namespace {

// Callbacks for testing ProcessRequestHeaders.
bool GetHeaderCallback(void* ctx,
                       const char* name,
                       char* value_buf,
                       size_t value_buf_size) {
  if (!name || !value_buf || value_buf_size == 0) {
    return false;
  }
  net::HttpRequestHeaders* headers = static_cast<net::HttpRequestHeaders*>(ctx);
  std::optional<std::string> value = headers->GetHeader(name);
  if (!value) {
    return false;
  }
  // SAFETY: This is a callback implementing the C-style GetHeaderFunction API.
  // We wrap the raw pointer and size in a base::span and use bounds-safe
  // operations for all copying.
  auto value_span = UNSAFE_BUFFERS(base::span(value_buf, value_buf_size));
  size_t copy_len = std::min(value->length(), value_buf_size - 1);
  value_span.first(copy_len).copy_from(base::span(*value).first(copy_len));
  value_span[copy_len] = '\0';
  return true;
}

void SetHeaderCallback(void* ctx, const char* name, const char* value) {
  net::HttpRequestHeaders* headers = static_cast<net::HttpRequestHeaders*>(ctx);
  headers->SetHeader(name, value);
}

}  // namespace

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
#if BUILDFLAG(IS_ANDROID)
  ASSERT_TRUE(base::PathService::Get(base::DIR_MODULE, &exe_path));
#elif !BUILDFLAG(IS_FUCHSIA)
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));
#endif

  base::FilePath library_path = exe_path.AppendASCII(
      base::GetNativeLibraryName("platform_runtime_test_lib"));

  runtime->UpdatePlatformRuntimeLibrary(library_path);
  scoped_refptr<PlatformRuntimeLibrary> loaded_lib =
      runtime->GetLoadedLibrary();
  ASSERT_NE(loaded_lib, nullptr);
  EXPECT_STREQ(loaded_lib->GetLibraryName(), "platform_runtime_test_lib");

  runtime->UpdatePlatformRuntimeLibrary(base::FilePath());
  EXPECT_EQ(runtime->GetLoadedLibrary(), nullptr);
}

TEST_F(PlatformRuntimeImplTest, ProcessRequestHeaders) {
  PlatformRuntimeImpl* runtime = PlatformRuntimeImpl::GetInstance();

  base::FilePath exe_path;
#if BUILDFLAG(IS_ANDROID)
  ASSERT_TRUE(base::PathService::Get(base::DIR_MODULE, &exe_path));
#elif !BUILDFLAG(IS_FUCHSIA)
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));
#endif

  base::FilePath library_path = exe_path.AppendASCII(
      base::GetNativeLibraryName("platform_runtime_test_lib"));

  runtime->UpdatePlatformRuntimeLibrary(library_path);
  scoped_refptr<PlatformRuntimeLibrary> loaded_lib =
      runtime->GetLoadedLibrary();
  ASSERT_NE(loaded_lib, nullptr);

  net::HttpRequestHeaders headers;
  headers.SetHeader("host", "example.com");

  // ProcessRequestHeaders should get "host" (example.com) and set a header
  // named "example.com" with the URL as value.
  bool result = loaded_lib->ProcessRequestHeaders(
      &headers, GetHeaderCallback, SetHeaderCallback, "http://newurl.com");

  EXPECT_TRUE(result);

  std::optional<std::string> value = headers.GetHeader("example.com");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, "http://newurl.com");
}

}  // namespace platform_runtime
