// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/ipc_security.h"

#include <windows.h>

#include <optional>

#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/win/access_token.h"
#include "chrome/updater/updater_scope.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "mojo/public/c/system/invitation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace {

constexpr wchar_t kTestServerName[] = L"IpcSecurityWinTestServerName";

}  // namespace

// Only a system install's server classifies its callers: it runs as SYSTEM and
// its pipe is open to any authenticated user. A user install's server and its
// callers run as the same user at the same integrity level, so there is
// nothing to defend and no caller is declared untrusted.
TEST(IpcSecurityWinTest, CallerClassificationIsSystemOnly) {
  const named_mojo_ipc_server::EndpointOptions user_options =
      CreateServerEndpointOptions(UpdaterScope::kUser, kTestServerName);
  EXPECT_FALSE(user_options.send_invitation_flags_callback);
  EXPECT_FALSE(user_options.include_peer_process_info);
  EXPECT_EQ(user_options.extra_send_invitation_flags,
            MOJO_SEND_INVITATION_FLAG_NONE);
  EXPECT_TRUE(user_options.security_descriptor.empty());

  const named_mojo_ipc_server::EndpointOptions system_options =
      CreateServerEndpointOptions(UpdaterScope::kSystem, kTestServerName);
  EXPECT_TRUE(system_options.send_invitation_flags_callback);
  // The callback reads the caller's token, so it needs the caller's process.
  EXPECT_TRUE(system_options.include_peer_process_info);
  EXPECT_FALSE(system_options.security_descriptor.empty());
}

// The endpoint's security is a function of the scope it was asked for, not of
// the scope this process happens to be running as. Production always passes
// the process's own scope, so this pins the coupling rather than guarding a
// reachable mismatch: it fails if the implementation goes back to reading the
// process-global `IsSystemInstall()`, since this test process is user scope.
TEST(IpcSecurityWinTest, OptionsFollowRequestedScopeNotProcessScope) {
  EXPECT_FALSE(CreateServerEndpointOptions(UpdaterScope::kUser, kTestServerName)
                   .send_invitation_flags_callback);
  EXPECT_TRUE(
      CreateServerEndpointOptions(UpdaterScope::kSystem, kTestServerName)
          .send_invitation_flags_callback);
}

// The protected endpoint is system-only by construction, so it classifies
// callers too. Its DACL admits only administrators and SYSTEM, so in practice
// every caller is trusted.
TEST(IpcSecurityWinTest, ProtectedEndpointClassifiesCallers) {
  const named_mojo_ipc_server::EndpointOptions options =
      CreateProtectedServerEndpointOptions(UpdaterScope::kSystem,
                                           kTestServerName);
  EXPECT_TRUE(options.send_invitation_flags_callback);
  EXPECT_TRUE(options.include_peer_process_info);
}

// A caller at High integrity or above (an elevated administrator, or SYSTEM)
// is at least as privileged as the server and keeps the behavior it had before
// callers were classified: it may pre-duplicate handles into the server. This
// is what keeps elevated callers built before that change - which do
// pre-duplicate, including during the connection handshake - able to connect.
// Everything below High integrity cannot open the SYSTEM server and so never
// pre-duplicates legitimately; it is declared untrusted so that the server
// refuses handle values it merely claims to have duplicated.
TEST(IpcSecurityWinTest, IntegrityLevelClassification) {
  EXPECT_EQ(SendInvitationFlagsForIntegrityLevel(SECURITY_MANDATORY_SYSTEM_RID),
            MOJO_SEND_INVITATION_FLAG_NONE);
  EXPECT_EQ(SendInvitationFlagsForIntegrityLevel(SECURITY_MANDATORY_HIGH_RID),
            MOJO_SEND_INVITATION_FLAG_NONE);
  EXPECT_EQ(SendInvitationFlagsForIntegrityLevel(SECURITY_MANDATORY_MEDIUM_RID),
            MOJO_SEND_INVITATION_FLAG_UNTRUSTED_PROCESS);
  EXPECT_EQ(SendInvitationFlagsForIntegrityLevel(SECURITY_MANDATORY_LOW_RID),
            MOJO_SEND_INVITATION_FLAG_UNTRUSTED_PROCESS);
  EXPECT_EQ(
      SendInvitationFlagsForIntegrityLevel(SECURITY_MANDATORY_UNTRUSTED_RID),
      MOJO_SEND_INVITATION_FLAG_UNTRUSTED_PROCESS);
  // No integrity level at all.
  EXPECT_EQ(SendInvitationFlagsForIntegrityLevel(MAXDWORD),
            MOJO_SEND_INVITATION_FLAG_UNTRUSTED_PROCESS);
}

// The per-caller classification reads the token of the process the server
// opened. This process stands in for a caller; whatever its integrity level
// is, the classification must agree with reading it directly.
TEST(IpcSecurityWinTest, CallerClassificationReadsCallerToken) {
  named_mojo_ipc_server::ConnectionInfo info;
  info.pid = base::GetCurrentProcId();
  info.process = base::Process::OpenWithAccess(
      info.pid, PROCESS_QUERY_LIMITED_INFORMATION);
  ASSERT_TRUE(info.process.IsValid());

  const std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromCurrentProcess();
  ASSERT_TRUE(token);
  EXPECT_EQ(SendInvitationFlagsForCaller(info),
            SendInvitationFlagsForIntegrityLevel(token->IntegrityLevel()));
}

// A caller whose process the server could not open is declared untrusted
// rather than trusted by default.
TEST(IpcSecurityWinTest, CallerWithoutProcessIsUntrusted) {
  named_mojo_ipc_server::ConnectionInfo info;
  ASSERT_FALSE(info.process.IsValid());
  EXPECT_EQ(SendInvitationFlagsForCaller(info),
            MOJO_SEND_INVITATION_FLAG_UNTRUSTED_PROCESS);
}

}  // namespace updater
