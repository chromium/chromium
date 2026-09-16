// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/shared_memory_user_stream_args.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "testing/gtest/include/gtest/gtest.h"

namespace crash_reporter::internal {

namespace {

constexpr char kHandlerName[] = "handler_name";
constexpr char kOtherArg[] = "--other-arg";

// Manually simulates OS multiprocess handle inheritance.
// In production, `AppendSharedMemoryUserStreamArgs` is called by the parent
// process and `ExtractSharedMemoryUserStreamArgs` by the child process. Because
// these tests run both within the same process, we must explicitly duplicate
// the handle in the CLI string before extraction to prevent closing the handle
// twice.
void DuplicateHandleInArg(std::string* arg) {
  if (!base::StartsWith(*arg, kUserStreamRegionSwitch)) {
    return;
  }

  std::string_view payload(*arg);
  payload.remove_prefix(kUserStreamRegionSwitch.length());

  const size_t comma_pos = payload.find(',');
  size_t handle_value = 0;
  CHECK(base::StringToSizeT(payload.substr(0, comma_pos), &handle_value));

  size_t dup_val = 0;
#if BUILDFLAG(IS_WIN)
  const HANDLE handle = reinterpret_cast<HANDLE>(handle_value);
  HANDLE dup_handle;
  CHECK(::DuplicateHandle(::GetCurrentProcess(), handle, ::GetCurrentProcess(),
                          &dup_handle, 0, FALSE, DUPLICATE_SAME_ACCESS));
  dup_val = reinterpret_cast<size_t>(dup_handle);
#else
  const int fd = static_cast<int>(handle_value);
  const int dup_fd = dup(fd);
  CHECK_GE(dup_fd, 0);
  dup_val = static_cast<size_t>(dup_fd);
#endif

  *arg = base::StrCat({kUserStreamRegionSwitch, base::NumberToString(dup_val),
                       payload.substr(comma_pos)});
}

}  // namespace

TEST(SharedMemoryUserStreamArgsTest, AppendAndExtractValidRegions) {
  constexpr size_t kRegion1Size = 128;
  constexpr size_t kRegion2Size = 256;

  auto region1 = base::ReadOnlySharedMemoryRegion::Create(kRegion1Size);
  auto region2 = base::ReadOnlySharedMemoryRegion::Create(kRegion2Size);
  ASSERT_TRUE(region1.IsValid());
  ASSERT_TRUE(region2.IsValid());

  std::vector<base::ReadOnlySharedMemoryRegion> regions;
  regions.push_back(std::move(region1.region));
  regions.push_back(std::move(region2.region));

  std::vector<std::string> args;
  std::set<crashpad::FileHandle> handles;
  AppendSharedMemoryUserStreamArgs(regions, &args, &handles);

  EXPECT_EQ(args.size(), 2u);
  EXPECT_EQ(handles.size(), 2u);

  std::vector<char*> argv;
  std::string handler_name = kHandlerName;
  argv.push_back(handler_name.data());
  for (auto& arg : args) {
    DuplicateHandleInArg(&arg);
    argv.push_back(arg.data());
  }
  std::string unrelated_arg = kOtherArg;
  argv.push_back(unrelated_arg.data());
  argv.push_back(nullptr);

  int argc = static_cast<int>(argv.size() - 1);
  const auto extracted = ExtractSharedMemoryUserStreamArgs(&argc, argv.data());

  EXPECT_EQ(argc, 2);
  EXPECT_EQ(std::string(argv[1]), kOtherArg);
  ASSERT_EQ(extracted.size(), 2u);
  EXPECT_EQ(extracted[0].GetSize(), kRegion1Size);
  EXPECT_EQ(extracted[1].GetSize(), kRegion2Size);
}

TEST(SharedMemoryUserStreamArgsTest, ExtractIgnoresInvalidArgs) {
  std::vector<std::string> args = {
      kHandlerName, base::StrCat({kUserStreamRegionSwitch, "invalid_format"}),
#if BUILDFLAG(IS_POSIX)
      // On Windows, passing a syntactically valid but completely fake, garbage
      // `HANDLE` to OS APIs (like `::GetHandleInformation`) can trigger fatal
      // structured exceptions. We only test OS rejection here on POSIX.
      base::StrCat({kUserStreamRegionSwitch, "999999,999999"}),
#endif
      base::StrCat({kUserStreamRegionSwitch, "999998,abc"}), kOtherArg};

  std::vector<char*> argv;
  for (auto& arg : args) {
    argv.push_back(arg.data());
  }
  argv.push_back(nullptr);

  int argc = static_cast<int>(argv.size() - 1);
  const auto extracted = ExtractSharedMemoryUserStreamArgs(&argc, argv.data());

  EXPECT_EQ(argc, 2);
  EXPECT_EQ(std::string(argv[0]), kHandlerName);
  EXPECT_EQ(std::string(argv[1]), kOtherArg);
  EXPECT_TRUE(extracted.empty());
}

TEST(SharedMemoryUserStreamArgsTest, AppendIgnoresInvalidRegions) {
  std::vector<base::ReadOnlySharedMemoryRegion> regions;
  regions.emplace_back();  // Invalid by default

  std::vector<std::string> args;
  std::set<crashpad::FileHandle> handles;
  AppendSharedMemoryUserStreamArgs(regions, &args, &handles);

  EXPECT_TRUE(args.empty());
  EXPECT_TRUE(handles.empty());
}

TEST(SharedMemoryUserStreamArgsTest, ExtractIgnoresNullptrArgs) {
  int argc = 0;
  auto extracted = ExtractSharedMemoryUserStreamArgs(&argc, nullptr);
  EXPECT_TRUE(extracted.empty());
  EXPECT_EQ(argc, 0);

  int null_argc = 5;
  extracted = ExtractSharedMemoryUserStreamArgs(&null_argc, nullptr);
  EXPECT_TRUE(extracted.empty());
  EXPECT_EQ(null_argc, 5);

  extracted = ExtractSharedMemoryUserStreamArgs(nullptr, nullptr);
  EXPECT_TRUE(extracted.empty());
}

TEST(SharedMemoryUserStreamArgsTest, ExtractIgnoresValidButNonSharedMemoryFd) {
#if BUILDFLAG(IS_WIN)
  // Create a valid OS handle that is intentionally not a shared
  // memory region (an `Event` object).
  const HANDLE fake_handle = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
  ASSERT_TRUE(fake_handle != nullptr && fake_handle != INVALID_HANDLE_VALUE);
  const size_t raw_handle_value = reinterpret_cast<size_t>(fake_handle);
#else
  // Create a valid OS file descriptor that is intentionally not a
  // shared memory region (a pipe).
  int pipe_fds[2];
  ASSERT_EQ(pipe(pipe_fds), 0);
  const int fake_fd = pipe_fds[1];
  close(pipe_fds[0]);
  const size_t raw_handle_value = static_cast<size_t>(fake_fd);
#endif

  std::vector<std::string> args = {
      kHandlerName,
      base::StrCat({kUserStreamRegionSwitch,
                    base::NumberToString(raw_handle_value), ",128"}),
      kOtherArg};

  std::vector<char*> argv;
  for (auto& arg : args) {
    argv.push_back(arg.data());
  }
  argv.push_back(nullptr);

  int argc = static_cast<int>(argv.size() - 1);
  const auto extracted = ExtractSharedMemoryUserStreamArgs(&argc, argv.data());

  EXPECT_EQ(argc, 2);
  EXPECT_EQ(std::string(argv[0]), kHandlerName);
  EXPECT_EQ(std::string(argv[1]), kOtherArg);
  EXPECT_TRUE(extracted.empty());

#if BUILDFLAG(IS_POSIX)
  // Because `fake_fd` was successfully parsed and wrapped in a `ScopedFD`
  // before `TakeOrFail` rejected its permissions, the `ScopedFD` should have
  // safely destroyed and closed the handle upon rejection.
  EXPECT_EQ(fcntl(fake_fd, F_GETFD), -1);
#endif
}

}  // namespace crash_reporter::internal
