// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/ipc_security.h"

#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <variant>

#include "build/build_config.h"
#include "chrome/updater/updater_scope.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "mojo/public/c/system/invitation.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include <bsm/libbsm.h>
#include <mach/mach.h>
#endif

namespace updater {

TEST(IpcSecurityPosixTest, CreateServerEndpointOptions) {
  for (UpdaterScope scope : {UpdaterScope::kUser, UpdaterScope::kSystem}) {
    const named_mojo_ipc_server::EndpointOptions options =
        CreateServerEndpointOptions(scope, "test_server_name");
    EXPECT_EQ(options.server_name, "test_server_name");
    EXPECT_TRUE(
        std::holds_alternative<std::monostate>(options.message_pipe_id));
    EXPECT_EQ(options.extra_send_invitation_flags,
              MOJO_SEND_INVITATION_FLAG_UNTRUSTED_PROCESS);
#if BUILDFLAG(IS_LINUX)
    EXPECT_FALSE(options.require_same_peer_user);
#endif
  }
}

TEST(IpcSecurityPosixTest, IsConnectionTrusted) {
  named_mojo_ipc_server::ConnectionInfo self_info;
  named_mojo_ipc_server::ConnectionInfo other_info;
#if BUILDFLAG(IS_LINUX)
  self_info.credentials.uid = geteuid();
  other_info.credentials.uid = geteuid() + 1;
#elif BUILDFLAG(IS_MAC)
  mach_msg_type_number_t count = TASK_AUDIT_TOKEN_COUNT;
  ASSERT_EQ(
      task_info(mach_task_self(), TASK_AUDIT_TOKEN,
                reinterpret_cast<task_info_t>(&self_info.audit_token), &count),
      KERN_SUCCESS);
  std::ranges::fill(other_info.audit_token.val,
                    static_cast<unsigned int>(geteuid() + 1));
#endif
  EXPECT_TRUE(IsConnectionTrusted(self_info));
  EXPECT_FALSE(IsConnectionTrusted(other_info));
}

}  // namespace updater
