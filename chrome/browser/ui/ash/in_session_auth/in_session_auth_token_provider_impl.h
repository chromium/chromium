// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_TOKEN_PROVIDER_IMPL_H_
#define CHROME_BROWSER_UI_ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_TOKEN_PROVIDER_IMPL_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/in_session_auth_token_provider.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {

class ASH_PUBLIC_EXPORT InSessionAuthTokenProviderImpl
    : public InSessionAuthTokenProvider {
 public:
  using OnAuthTokenGenerated = InSessionAuthTokenProvider::OnAuthTokenGenerated;

  InSessionAuthTokenProviderImpl();
  ~InSessionAuthTokenProviderImpl() override = default;

  // InSessionAuthTokenProvider
  void ExchangeForToken(std::unique_ptr<UserContext> user_context,
                        OnAuthTokenGenerated callback) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_TOKEN_PROVIDER_IMPL_H_
