// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/test_support/scoped_medium_integrity.h"

#include <windows.h>

#include <optional>
#include <ostream>

#include "base/logging.h"
#include "base/win/access_token.h"
#include "testing/gtest/include/gtest/gtest.h"

ScopedMediumIntegrity::ScopedMediumIntegrity() {
  // Ensure that GoogleTest has created at least one ThreadLocal on this thread
  // before impersonating. Otherwise, a failure in the test could lead to an
  // assertion failure when GoogleTest tries to get a handle to the current
  // thread.
  { SCOPED_TRACE(""); }
  if (auto process_token = base::win::AccessToken::FromCurrentProcess(
          /*impersonation=*/false, TOKEN_DUPLICATE);
      !process_token.has_value()) {
    PLOG(ERROR) << "Failed to duplicate process token";
  } else if (auto lua_token = process_token->CreateRestricted(
                 LUA_TOKEN, /*sids_to_disable=*/{},
                 /*privileges_to_delete=*/{},
                 /*sids_to_restrict=*/{}, TOKEN_ALL_ACCESS);
             !lua_token.has_value()) {
    PLOG(ERROR) << "Failed to create LUA token";
  } else if (!lua_token->SetIntegrityLevel(SECURITY_MANDATORY_MEDIUM_RID)) {
    PLOG(ERROR) << "Failed to set integrity level on UIA token";
  } else if (auto imp_token = lua_token->DuplicateImpersonation(
                 base::win::SecurityImpersonationLevel::kImpersonation,
                 TOKEN_ALL_ACCESS);
             !imp_token.has_value()) {
    PLOG(ERROR) << "Failed to create impersonation token";
  } else if (!::ImpersonateLoggedOnUser(imp_token->get())) {
    PLOG(ERROR) << "Failed to impersonate";
  } else {
    impersonating_ = true;
  }
}

ScopedMediumIntegrity::~ScopedMediumIntegrity() {
  if (impersonating_ && !::RevertToSelf()) {
    PLOG(ERROR) << "Failed to revert to self";
  }
}
