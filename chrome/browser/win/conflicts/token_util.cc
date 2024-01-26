// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/token_util.h"

#include <optional>

#include "base/win/access_token.h"
#include "base/win/sid.h"

bool HasAdminRights() {
  std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromCurrentProcess(/*impersonation=*/true);
  if (!token)
    return false;
  if (token->IsMember(base::win::WellKnownSid::kBuiltinAdministrators))
    return true;

  // In the case that UAC is enabled, it's possible that the current token is
  // filtered. So check the linked token in case it is a member of the built-in
  // Administrators group.
  std::optional<base::win::AccessToken> linked_token = token->LinkedToken();
  if (!linked_token)
    return false;
  return linked_token->IsMember(
      base::win::WellKnownSid::kBuiltinAdministrators);
}
