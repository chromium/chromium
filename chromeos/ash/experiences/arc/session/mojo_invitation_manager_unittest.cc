// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/session/mojo_invitation_manager.h"

#include <fcntl.h>
#include <signal.h>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "chromeos/ash/experiences/arc/session/mojo_init_data.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace arc {
namespace {

TEST(ArcMojoInvitationTest, MojoProxyIsLaunched) {
  base::CommandLine command_line =
      base::GetMultiProcessTestChildBaseCommandLine();
  base::LaunchOptions launch_options;
  base::Process process = base::SpawnMultiProcessTestChild(
      "InvitationSenderMain", command_line, launch_options);

  int rv = -1;
  process.WaitForExitWithTimeout(TestTimeouts::action_timeout(), &rv);
  EXPECT_EQ(0, rv);
}

MULTIPROCESS_TEST_MAIN(InvitationSenderMain) {
  base::test::TaskEnvironment task_environment;
  base::Thread io_thread{"IO thread"};
  io_thread.StartWithOptions(
      base::Thread::Options{base::MessagePumpType::IO, 0});

  auto ipc_support = std::make_unique<mojo::core::ScopedIPCSupport>(
      io_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  {
    MojoInvitationManager invitation_manager(base::FilePath("/bin/true"));
    mojo::PlatformChannel channel;
    MojoInitData mojo_init_data;
    invitation_manager.SendInvitation(channel, mojo_init_data.token());

    // Verify that (fake) proxy process was launched successfully.
    base::Process proxy_process = std::move(invitation_manager.proxy_process());
    CHECK(proxy_process.IsValid());
    int rv = -1;
    proxy_process.WaitForExit(&rv);
    CHECK_EQ(0, rv);
  }

  // Verify that destroying MojoInvitationManager while the proxy process is
  // still running terminates and reaps it. The fake proxy writes a byte to the
  // remapped legacy client FD (3) to signal that it is running.
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());
  base::FilePath fake_proxy_path =
      temp_dir.GetPath().Append("fake_mojo_proxy.sh");
  CHECK(base::WriteFile(fake_proxy_path,
                        "#!/bin/sh\nprintf x >&3\nexec /bin/sleep 60\n"));
  CHECK(base::SetPosixFilePermissions(
      fake_proxy_path, base::FILE_PERMISSION_READ_BY_USER |
                           base::FILE_PERMISSION_WRITE_BY_USER |
                           base::FILE_PERMISSION_EXECUTE_BY_USER));

  base::ProcessId proxy_pid = base::kNullProcessId;
  {
    MojoInvitationManager invitation_manager(fake_proxy_path);
    mojo::PlatformChannel channel;
    MojoInitData mojo_init_data;
    invitation_manager.SendInvitation(channel, mojo_init_data.token());
    CHECK(invitation_manager.proxy_process().IsValid());
    base::ScopedFD remote_fd =
        channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD();
    CHECK_EQ(0, fcntl(remote_fd.get(), F_SETFL, 0));
    char ready = 0;
    CHECK(base::ReadFromFD(remote_fd.get(), base::span_from_ref(ready)));
    CHECK_EQ('x', ready);
    proxy_pid = invitation_manager.proxy_process().Pid();
  }
  base::ThreadPoolInstance::Get()->FlushForTesting();
  CHECK_NE(0, kill(proxy_pid, 0));

  ipc_support.reset();
  io_thread.Stop();
  mojo::core::ShutDown();

  return 0;
}

}  // namespace
}  // namespace arc
