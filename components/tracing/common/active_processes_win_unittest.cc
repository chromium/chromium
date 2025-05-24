// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/active_processes_win.h"

#include <windows.h>

#include <optional>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process/process.h"
#include "base/test/scoped_path_override.h"
#include "base/threading/platform_thread.h"
#include "base/version_info/version_info_values.h"
#include "base/win/access_token.h"
#include "base/win/sid.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

class ActiveProcessesTest : public testing::Test {
 protected:
  // ActiveProcesses explicitly uses uint32_t for pid/tid, rather than the
  // base typedefs for process/thread id. Assert that these match in size.
  using ProcessId = uint32_t;
  using ThreadId = uint32_t;
  static_assert(sizeof(ProcessId) == sizeof(base::ProcessId));
  static_assert(sizeof(ThreadId) ==
                sizeof(base::PlatformThreadId::UnderlyingType));

  using PidAndTid = std::pair<ProcessId, ThreadId>;

  // The pid and tid of the client. Named "base" because the offsets below are
  // applied to compute distinct pids and tids for other processes.
  static constexpr ProcessId kBasePid = 0x1000;
  static constexpr ThreadId kBaseTid = 0x1000;

  // Offsets from the bases above for pids and tids for other processes.
  static constexpr int kChildOffset = 100;
  static constexpr int kUnrelatedOffset = 200;
  static constexpr int kIndependentOffset = 300;
  static constexpr int kSystemOffset = 400;

  // testing::Test:
  void SetUp() override {
    // Create a version directory %TMP%\random\W.X.Y.Z.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    fake_dir_exe_ =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL(PRODUCT_VERSION));
    ASSERT_TRUE(base::CreateDirectory(fake_dir_exe_));

    // Pretend that it is the dir in which the current executable is running.
    dir_exe_override_.emplace(base::DIR_EXE, fake_dir_exe_,
                              /*is_absolute=*/true, /*create=*/false);

    // Make a command line for a client residing in %TMP%\random.
    base::CommandLine client_command_line(
        temp_dir_.GetPath().AppendASCII(client_image_file_name_));
    client_command_line.AppendArgNative(L"arg1");
    client_command_line.AppendSwitchNative("switch1", L"value1");
    client_command_line.AppendSwitch("switch2");
    client_command_line.AppendArgNative(L"arg2");
    client_command_line_ = client_command_line.GetCommandLineString();

    // Make a command line for an external client residing in %TMP%.
    external_client_command_line_ =
        base::CommandLine(
            temp_dir_.GetPath().DirName().AppendASCII(client_image_file_name_))
            .GetCommandLineString();

    // Now construct the instance to test.
    active_processes_.emplace(kBasePid);
  }

  ActiveProcesses& active_processes() { return *active_processes_; }

  // Some random sid.
  const base::win::Sid& random_user_sid() const { return random_user_sid_; }

  // The file name of the current process's image.
  const std::string& client_image_file_name() const {
    return client_image_file_name_;
  }

  // The command line string of a client that belongs to the application.
  const std::wstring& client_command_line() const {
    return client_command_line_;
  }

  // The command line string of a client that is unrelated to the application.
  const std::wstring& external_client_command_line() const {
    return external_client_command_line_;
  }

  // Adds the client.
  PidAndTid AddClient() {
    active_processes().AddProcess(
        kBasePid, /*parent_pid=*/0, /*session_id=*/4, random_user_sid().Clone(),
        client_image_file_name(), client_command_line());
    active_processes().AddThread(kBasePid, kBaseTid, /*thread_name=*/{});
    return {kBasePid, kBaseTid};
  }

  // Adds a child process of the client -- same image file name, parent pid
  // matches the client.
  PidAndTid AddChild() {
    const ProcessId child_pid = kBasePid + kChildOffset;
    const ThreadId child_tid = kBaseTid + kChildOffset;
    active_processes().AddProcess(
        child_pid, kBasePid, /*session_id=*/4, random_user_sid().Clone(),
        client_image_file_name(), client_command_line());
    active_processes().AddThread(child_pid, child_tid,
                                 /*thread_name=*/{});
    return {child_pid, child_tid};
  }

  // Adds a process unrelated to the client and returns its pid and a tid.
  // `unrelated_pid` may be set to force a specific pid to be used.
  PidAndTid AddUnrelated(ProcessId unrelated_pid = kBasePid +
                                                   kUnrelatedOffset) {
    base::CommandLine unrelated_command_line(base::CommandLine::NO_PROGRAM);
    unrelated_command_line.ParseFromString(client_command_line());
    unrelated_command_line.SetProgram(
        unrelated_command_line.GetProgram().DirName().DirName().Append(
            FILE_PATH_LITERAL("unrelated.exe")));

    const ThreadId unrelated_tid = kBaseTid + kUnrelatedOffset;
    active_processes().AddProcess(
        unrelated_pid, /*parent_pid=*/0, /*session_id=*/4,
        random_user_sid().Clone(), "unrelated.exe",
        unrelated_command_line.GetCommandLineString());
    active_processes().AddThread(unrelated_pid, unrelated_tid,
                                 /*thread_name=*/{});
    return {unrelated_pid, unrelated_tid};
  }

  // Adds an independent process of the client -- in the same directory tree.
  PidAndTid AddIndependent() {
    base::CommandLine independent_command_line(base::CommandLine::NO_PROGRAM);
    independent_command_line.ParseFromString(client_command_line());
    independent_command_line.SetProgram(
        independent_command_line.GetProgram()
            .DirName()
            .Append(FILE_PATH_LITERAL("1.2.3"))
            .Append(FILE_PATH_LITERAL("independent.exe")));

    const ProcessId independent_pid = kBasePid + kIndependentOffset;
    const ThreadId independent_tid = kBaseTid + kIndependentOffset;
    active_processes().AddProcess(
        independent_pid, /*parent_pid=*/0, /*session_id=*/4,
        random_user_sid().Clone(), "independent.exe",
        independent_command_line.GetCommandLineString());
    active_processes().AddThread(independent_pid, independent_tid,
                                 /*thread_name=*/{});
    return {independent_pid, independent_tid};
  }

 private:
  const std::string client_image_file_name_{"client.exe"};
  const base::win::Sid random_user_sid_{base::win::Sid::GenerateRandomSid()};
  base::ScopedTempDir temp_dir_;
  base::FilePath fake_dir_exe_;
  std::optional<base::ScopedPathOverride> dir_exe_override_;
  std::wstring client_command_line_;
  std::wstring external_client_command_line_;

  // The base pid identifies the tracing client.
  std::optional<ActiveProcesses> active_processes_;
};

TEST_F(ActiveProcessesTest, UnknownTidIsOther) {
  ASSERT_EQ(active_processes().GetThreadCategory(kBaseTid),
            ActiveProcesses::Category::kOther);
}

// Tests that a thread added for an unknown process is considered an "other"
// process.
TEST_F(ActiveProcessesTest, TidForUnknownPidIsOther) {
  active_processes().AddThread(kBasePid, kBaseTid, /*thread_name=*/{});
  ASSERT_EQ(active_processes().GetThreadCategory(kBaseTid),
            ActiveProcesses::Category::kOther);
}

TEST_F(ActiveProcessesTest, ClientIsClient) {
  // Add the client process and a thread, and confirm that it is categorized
  // properly.
  const auto [client_pid, client_tid] = AddClient();
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kClient);
}

// Tests that categorization of the client works as threads come and go.
TEST_F(ActiveProcessesTest, ClientThreadActivity) {
  // Add the client and a couple of threads.
  const auto [client_pid, client_tid] = AddClient();
  const auto client_tid2 = client_tid + 1;
  active_processes().AddThread(client_pid, client_tid2, /*thread_name=*/{});

  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kClient);
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid2),
            ActiveProcesses::Category::kClient);

  // Now remove a thread.
  active_processes().RemoveThread(client_pid, client_tid2);
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kClient);
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid2),
            ActiveProcesses::Category::kOther);

  // Now remove the process.
  active_processes().RemoveProcess(client_pid);
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kOther);
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid2),
            ActiveProcesses::Category::kOther);
}

// Tests that a child process of the client using the same image name is
// categorized as a client proc.
TEST_F(ActiveProcessesTest, ChildOfClient) {
  // Add the client.
  const auto [client_pid, client_tid] = AddClient();

  // Add a child process of the client -- same image file name, parent pid
  // matches the client.
  const auto [child_pid, child_tid] = AddChild();

  // The child is categorized as a Client process.
  ASSERT_EQ(active_processes().GetThreadCategory(child_tid),
            ActiveProcesses::Category::kClient);

  // The child going away shouldn't change the categorization of the client.
  active_processes().RemoveProcess(child_pid);
  ASSERT_EQ(active_processes().GetThreadCategory(child_tid),
            ActiveProcesses::Category::kOther);
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kClient);
}

// Tests that a child process of the client using the same image name is
// categorized as a client proc once the client is added.
TEST_F(ActiveProcessesTest, ClientAfterChild) {
  // Add a child process of the client -- same image file name, parent pid
  // matches the client.
  const auto [child_pid, child_tid] = AddChild();

  // The client hasn't been added, so the child is still "other".
  ASSERT_EQ(active_processes().GetThreadCategory(child_tid),
            ActiveProcesses::Category::kOther);

  // Add the client.
  const auto [client_pid, client_tid] = AddClient();
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kClient);

  // Now the child is categorized as a Client process.
  ASSERT_EQ(active_processes().GetThreadCategory(child_tid),
            ActiveProcesses::Category::kClient);

  // When the client goes away, the child is recategorized as "other".
  active_processes().RemoveProcess(client_pid);
  ASSERT_EQ(active_processes().GetThreadCategory(child_tid),
            ActiveProcesses::Category::kOther);
}

// Tests that a process unrelated to the client is categorized as "other".
TEST_F(ActiveProcessesTest, UnrelatedProcess) {
  // Add a process unrelated to the client -- in some other directory tree.
  const auto [unrelated_pid, unrelated_tid] = AddUnrelated();
  ASSERT_EQ(active_processes().GetThreadCategory(unrelated_tid),
            ActiveProcesses::Category::kOther);

  // Add the client.
  const auto [client_pid, client_tid] = AddClient();
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kClient);

  // The unrelated process is still categorized as "other".
  ASSERT_EQ(active_processes().GetThreadCategory(unrelated_tid),
            ActiveProcesses::Category::kOther);
}

// Tests that a process whose program resides within the directory tree of the
// client is categorized as a client proc.
TEST_F(ActiveProcessesTest, ProgramOfClient) {
  // Add the client.
  const auto [client_pid, client_tid] = AddClient();
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kClient);

  // Add an independent process of the client -- in the same directory tree.
  const auto [independent_pid, independent_tid] = AddIndependent();

  // The independent is categorized as a Client process.
  ASSERT_EQ(active_processes().GetThreadCategory(independent_tid),
            ActiveProcesses::Category::kClient);

  // The independent going away shouldn't change the categorization of the
  // client.
  active_processes().RemoveProcess(independent_pid);
  ASSERT_EQ(active_processes().GetThreadCategory(independent_tid),
            ActiveProcesses::Category::kOther);
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kClient);
}

// Tests that a process whose program resides within the directory tree of the
// client is categorized as a client proc once the client is added.
TEST_F(ActiveProcessesTest, ClientAfterProgram) {
  // Add an independent process of the client -- in the same directory tree.
  const auto [independent_pid, independent_tid] = AddIndependent();

  // The client hasn't been added, so the independent is still "other".
  ASSERT_EQ(active_processes().GetThreadCategory(independent_tid),
            ActiveProcesses::Category::kOther);

  // Add the client.
  const auto [client_pid, client_tid] = AddClient();
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kClient);

  // Now the independent is categorized as a Client process.
  ASSERT_EQ(active_processes().GetThreadCategory(independent_tid),
            ActiveProcesses::Category::kClient);

  // When the client goes away, the independent is recategorized as "other".
  active_processes().RemoveProcess(client_pid);
  ASSERT_EQ(active_processes().GetThreadCategory(independent_tid),
            ActiveProcesses::Category::kOther);
}

// Tests that a process matching the client, but running as another user, is
// categorized as "other".
TEST_F(ActiveProcessesTest, ClientOtherUser) {
  // Add the client.
  const auto [client_pid, client_tid] = AddClient();
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kClient);

  // Add another instance of the client running as another user.
  const ProcessId other_user_pid = client_pid + 1;
  const ThreadId other_user_tid = client_tid + 1;
  active_processes().AddProcess(
      other_user_pid, /*parent_pid=*/0, /*session_id=*/4,
      base::win::Sid::GenerateRandomSid(), client_image_file_name(),
      client_command_line());
  active_processes().AddThread(other_user_pid, other_user_tid,
                               /*thread_name=*/{});

  // This process running as another user is categorized as "other".
  ASSERT_EQ(active_processes().GetThreadCategory(other_user_tid),
            ActiveProcesses::Category::kOther);
}

// Tests that a process matching the client, but running in another window
// session, is categorized as "other".
TEST_F(ActiveProcessesTest, ClientOtherSession) {
  // Add the client.
  const auto [client_pid, client_tid] = AddClient();
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kClient);

  // Add another instance of the client running in a different session
  const ProcessId other_session_pid = client_pid + 1;
  const ThreadId other_session_tid = client_tid + 1;
  active_processes().AddProcess(other_session_pid, /*parent_pid=*/0,
                                /*session_id=*/5, random_user_sid().Clone(),
                                client_image_file_name(),
                                client_command_line());
  active_processes().AddThread(other_session_pid, other_session_tid,
                               /*thread_name=*/{});

  // This process running in another session is categorized as "other".
  ASSERT_EQ(active_processes().GetThreadCategory(other_session_tid),
            ActiveProcesses::Category::kOther);
}

// Tests that a client that is external to the application is considered the
// client and that other processes belonging to the application are not.
TEST_F(ActiveProcessesTest, ExternalClient) {
  // Add a client that does not reside in the application's directory.
  const ProcessId client_pid = kBasePid;
  const ThreadId client_tid = kBaseTid;
  active_processes().AddProcess(
      client_pid, /*parent_pid=*/0, /*session_id=*/4, random_user_sid().Clone(),
      client_image_file_name(), external_client_command_line());
  active_processes().AddThread(client_pid, client_tid, /*thread_name=*/{});
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kClient);

  // If it launches a child of the application, those children are considered
  // "other" with respect to the client.
  const ProcessId child_pid = kBasePid + kChildOffset;
  const ThreadId child_tid = kBaseTid + kChildOffset;
  active_processes().AddProcess(
      child_pid, client_pid, /*session_id=*/4, random_user_sid().Clone(),
      client_image_file_name(), client_command_line());
  active_processes().AddThread(child_pid, child_tid, /*thread_name=*/{});
  ASSERT_EQ(active_processes().GetThreadCategory(child_tid),
            ActiveProcesses::Category::kOther);
}

// Tests that a system process is categorized as "other".
TEST_F(ActiveProcessesTest, SystemProcess) {
  // Any process running in session 0xFFFFFFFF is a system process.
  const ProcessId system_pid = kBasePid + kSystemOffset;
  const ThreadId system_tid = kBaseTid + kSystemOffset;
  active_processes().AddProcess(system_pid, /*parent_pid=*/0,
                                /*session_id=*/0xFFFFFFFF,
                                base::win::Sid::GenerateRandomSid(), "system",
                                /*command_line=*/{});
  active_processes().AddThread(system_pid, system_tid, /*thread_name=*/{});
  ASSERT_EQ(active_processes().GetThreadCategory(system_tid),
            ActiveProcesses::Category::kSystem);
}

// Tests that inconsistencies from lost events are handled gracefully.
TEST_F(ActiveProcessesTest, LostEvents) {
  // Add the client, a child of it, and an unrelated process.
  const auto [client_pid, client_tid] = AddClient();
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kClient);

  const auto [child_pid, child_tid] = AddChild();
  ASSERT_EQ(active_processes().GetThreadCategory(child_tid),
            ActiveProcesses::Category::kClient);

  const auto [unrelated_pid, unrelated_tid] = AddUnrelated();
  ASSERT_EQ(active_processes().GetThreadCategory(unrelated_tid),
            ActiveProcesses::Category::kOther);

  // Removing an unknown tid from an unknown pid is a noop.
  active_processes().RemoveThread(client_pid + 1, client_tid + 1);
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kClient);
  ASSERT_EQ(active_processes().GetThreadCategory(child_tid),
            ActiveProcesses::Category::kClient);

  // Removing an unknown tid from a known pid is a noop.
  active_processes().RemoveThread(client_pid, client_tid + 1);
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kClient);
  ASSERT_EQ(active_processes().GetThreadCategory(child_tid),
            ActiveProcesses::Category::kClient);

  // Removing a known tid from an unknown pid is a noop.
  active_processes().RemoveThread(client_pid + 1, client_tid);
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kClient);
  ASSERT_EQ(active_processes().GetThreadCategory(child_tid),
            ActiveProcesses::Category::kClient);

  // Adding an existing tid to a different process removes the tid from the
  // original.
  active_processes().AddThread(client_pid, client_tid + 1, /*thread_name=*/{});
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid + 1),
            ActiveProcesses::Category::kClient);
  active_processes().AddThread(unrelated_pid, client_tid + 1,
                               /*thread_name=*/{});
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid + 1),
            ActiveProcesses::Category::kOther);

  // Removing a pid also removes its tids.
  active_processes().RemoveProcess(client_pid);
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kOther);
}

// Tests that a second process added with the same pid as a child of the client
// leads to the child no longer categorized as a client proc.
TEST_F(ActiveProcessesTest, DuplicatePid) {
  // Add the client and a child of it.
  const auto [client_pid, client_tid] = AddClient();
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kClient);
  const auto [child_pid, child_tid] = AddChild();
  ASSERT_EQ(active_processes().GetThreadCategory(child_tid),
            ActiveProcesses::Category::kClient);

  // Imagine that events removing the child's thread and process are lost, and
  // its pid is reused for a new process that is not related to the client.
  const auto [unrelated_pid, unrelated_tid] = AddUnrelated(child_pid);
  ASSERT_EQ(unrelated_pid, child_pid);
  ASSERT_EQ(active_processes().GetThreadCategory(unrelated_tid),
            ActiveProcesses::Category::kOther);
  ASSERT_NE(unrelated_tid, child_tid);

  // The tid of the replaced child is no longer considered related to the
  // client.
  ASSERT_EQ(active_processes().GetThreadCategory(child_tid),
            ActiveProcesses::Category::kOther);
}

// Tests that a second process added with the same pid as the client leads to
// the client being forgotten.
TEST_F(ActiveProcessesTest, DuplicateClient) {
  // Add the client.
  const auto [client_pid, client_tid] = AddClient();
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kClient);

  // Imagine a new process with the same pid as the client is added. This should
  // not be possible in practice since the tracing service observes termination
  // of the client and exits, but a shutdown race plus lost events could make an
  // unlikely event possible. The new process should be considered as "other".
  const auto [unrelated_pid, unrelated_tid] = AddUnrelated(client_pid);
  ASSERT_EQ(unrelated_pid, client_pid);
  ASSERT_EQ(active_processes().GetThreadCategory(unrelated_tid),
            ActiveProcesses::Category::kOther);

  // The tid of the replaced process is no longer considered the client.
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kOther);
}

// Tests that SetThreadName works in the general case, and in the face of lost
// events.
TEST_F(ActiveProcessesTest, SetThreadName) {
  // Getters return empty view for unknown inputs, while setter does nothing.
  ASSERT_EQ(active_processes().GetThreadName(3), L"");
  active_processes().SetThreadName(/*pid=*/2, /*tid=*/3, L"Hi");
  ASSERT_EQ(active_processes().GetThreadName(3), L"");

  // Add the client and a child.
  const auto [client_pid, client_tid] = AddClient();
  ASSERT_EQ(active_processes().GetThreadCategory(client_tid),
            ActiveProcesses::Category::kClient);
  const auto [child_pid, child_tid] = AddChild();
  ASSERT_EQ(active_processes().GetThreadCategory(child_tid),
            ActiveProcesses::Category::kClient);

  // Set works on valid inputs.
  ASSERT_EQ(active_processes().GetThreadName(client_tid), std::wstring_view());
  active_processes().SetThreadName(client_pid, client_tid, L"Hi");
  ASSERT_EQ(active_processes().GetThreadName(client_tid), L"Hi");

  // Set is ignored for pid/tid mismatch.
  active_processes().SetThreadName(child_pid, client_tid, L"Bye");
  ASSERT_EQ(active_processes().GetThreadName(client_tid), L"Hi");

  // Set is ignore for invalid tid.
  active_processes().SetThreadName(client_pid, client_tid + 1, L"Bye");
  ASSERT_EQ(active_processes().GetThreadName(client_tid), L"Hi");
  ASSERT_EQ(active_processes().GetThreadName(client_tid + 1), L"");

  // Set is ignore for invalid pid.
  active_processes().SetThreadName(client_pid + 1, client_tid, L"Bye");
  ASSERT_EQ(active_processes().GetThreadName(client_tid), L"Hi");
}

}  // namespace tracing
