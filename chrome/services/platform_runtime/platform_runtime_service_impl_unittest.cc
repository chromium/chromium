// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/platform_runtime/platform_runtime_service_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/common/request_header_integrity/platform_runtime.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace platform_runtime {

namespace {

using request_header_integrity::mojom::PlatformRuntimeStatus;

// The value platform_runtime_test_lib writes when the service passes no URL.
constexpr char kTestLibNoUrlValue[] = "no-url";

base::FilePath TestLibraryPath() {
  base::FilePath exe_path;
  CHECK(base::PathService::Get(base::DIR_EXE, &exe_path));
  return exe_path.AppendASCII(
      base::GetNativeLibraryName("platform_runtime_test_lib"));
}

net::HttpRequestHeaders MakeHeaders(
    const std::vector<std::pair<std::string, std::string>>& headers) {
  net::HttpRequestHeaders result;
  for (const auto& [key, value] : headers) {
    result.SetHeader(key, value);
  }
  return result;
}

}  // namespace

// Drives PlatformRuntimeServiceImpl the way the browser does: over a real
// message pipe, so the mojom's serialization is exercised along with the
// service logic.
class PlatformRuntimeServiceImplTest : public testing::Test {
 protected:
  void SetUp() override {
    service_ = std::make_unique<PlatformRuntimeServiceImpl>(
        remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    remote_.reset();
    service_.reset();
  }

  mojo::Remote<request_header_integrity::mojom::PlatformRuntime> LoadLibrary(
      const base::FilePath& library_path) {
    mojo::Remote<request_header_integrity::mojom::PlatformRuntime> runtime;
    remote_->LoadLibrary(library_path, runtime.BindNewPipeAndPassReceiver());
    return runtime;
  }

  using ProcessResult =
      base::expected<net::HttpRequestHeaders, PlatformRuntimeStatus>;

  // Runs one round trip and returns the reply.
  ProcessResult ProcessHeaders(
      request_header_integrity::mojom::PlatformRuntime* runtime,
      const net::HttpRequestHeaders& input_headers) {
    base::test::TestFuture<ProcessResult> future;
    runtime->ProcessHeaders(input_headers, future.GetCallback());
    return future.Take();
  }

  base::test::TaskEnvironment task_environment_;
  mojo::Remote<request_header_integrity::mojom::PlatformRuntimeService> remote_;
  std::unique_ptr<PlatformRuntimeServiceImpl> service_;
};

TEST_F(PlatformRuntimeServiceImplTest, LibraryUnavailable) {
  auto runtime =
      LoadLibrary(base::FilePath(FILE_PATH_LITERAL("non_existent_library")));
  EXPECT_EQ(
      ProcessHeaders(runtime.get(), MakeHeaders({{"host", "example.com"}})),
      base::unexpected(PlatformRuntimeStatus::kLibraryUnavailable));
}

TEST_F(PlatformRuntimeServiceImplTest, Success) {
  // The test library reads "host" and writes a header named after its value.
  auto runtime = LoadLibrary(TestLibraryPath());
  ProcessResult result =
      ProcessHeaders(runtime.get(), MakeHeaders({{"host", "example.com"}}));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->GetHeader("example.com"), kTestLibNoUrlValue);
}

TEST_F(PlatformRuntimeServiceImplTest, LibraryReportsFailure) {
  // Without a "host" header the test library's get_header call fails and it
  // reports failure.
  auto runtime = LoadLibrary(TestLibraryPath());
  EXPECT_EQ(
      ProcessHeaders(runtime.get(), MakeHeaders({{"accept", "text/html"}})),
      base::unexpected(PlatformRuntimeStatus::kFailure));
}

TEST_F(PlatformRuntimeServiceImplTest, RanButWroteNothing) {
  // The library succeeds but every header it writes is rejected by the
  // service's validation, which is not the same thing as success.
  auto runtime = LoadLibrary(TestLibraryPath());
  EXPECT_EQ(ProcessHeaders(runtime.get(),
                           MakeHeaders({{"host", "not a valid header name"}})),
            base::unexpected(PlatformRuntimeStatus::kNoOutput));
}

TEST_F(PlatformRuntimeServiceImplTest, LibraryIsReusedAcrossCalls) {
  auto runtime = LoadLibrary(TestLibraryPath());

  EXPECT_TRUE(
      ProcessHeaders(runtime.get(), MakeHeaders({{"host", "first.test"}}))
          .has_value());

  ProcessResult result =
      ProcessHeaders(runtime.get(), MakeHeaders({{"host", "second.test"}}));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->GetHeader("second.test"), kTestLibNoUrlValue);
}

TEST_F(PlatformRuntimeServiceImplTest, ReloadsAfterFailedLoad) {
  auto bad_runtime =
      LoadLibrary(base::FilePath(FILE_PATH_LITERAL("non_existent_library")));
  EXPECT_EQ(
      ProcessHeaders(bad_runtime.get(), MakeHeaders({{"host", "example.com"}})),
      base::unexpected(PlatformRuntimeStatus::kLibraryUnavailable));

  auto good_runtime = LoadLibrary(TestLibraryPath());
  EXPECT_TRUE(
      ProcessHeaders(good_runtime.get(), MakeHeaders({{"host", "example.com"}}))
          .has_value());
}

}  // namespace platform_runtime
