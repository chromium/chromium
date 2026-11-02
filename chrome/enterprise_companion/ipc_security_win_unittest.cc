// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/ipc_security.h"

#include <windows.h>

#include <optional>

#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/win/access_token.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_companion {

TEST(IpcSecurityWinTest, IntegrityLevelClassification) {
  EXPECT_TRUE(IsTrustedIntegrityLevel(SECURITY_MANDATORY_SYSTEM_RID));
  EXPECT_TRUE(IsTrustedIntegrityLevel(SECURITY_MANDATORY_HIGH_RID));
  EXPECT_FALSE(IsTrustedIntegrityLevel(SECURITY_MANDATORY_MEDIUM_PLUS_RID));
  EXPECT_FALSE(IsTrustedIntegrityLevel(SECURITY_MANDATORY_MEDIUM_RID));
  EXPECT_FALSE(IsTrustedIntegrityLevel(SECURITY_MANDATORY_LOW_RID));
  EXPECT_FALSE(IsTrustedIntegrityLevel(SECURITY_MANDATORY_UNTRUSTED_RID));
  // No integrity level at all.
  EXPECT_FALSE(IsTrustedIntegrityLevel(MAXDWORD));
}

TEST(IpcSecurityWinTest, TrustDeciderReadsCallerToken) {
  named_mojo_ipc_server::ConnectionInfo info;
  info.pid = base::GetCurrentProcId();
  info.process = base::Process::OpenWithAccess(
      info.pid, PROCESS_QUERY_LIMITED_INFORMATION);
  ASSERT_TRUE(info.process.IsValid());

  const std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromCurrentProcess();
  ASSERT_TRUE(token);
  EXPECT_EQ(CreateIpcTrustDecider().Run(info),
            IsTrustedIntegrityLevel(token->IntegrityLevel()));
}

TEST(IpcSecurityWinTest, CallerWithoutProcessIsUntrusted) {
  named_mojo_ipc_server::ConnectionInfo info;
  info.pid = base::GetCurrentProcId();
  ASSERT_FALSE(info.process.IsValid());
  EXPECT_FALSE(CreateIpcTrustDecider().Run(info));
}

}  // namespace enterprise_companion
