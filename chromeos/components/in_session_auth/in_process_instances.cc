// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/in_session_auth/in_process_instances.h"
#include "chromeos/components/in_session_auth/in_session_auth.h"

namespace chromeos::auth {

namespace {

InSessionAuth* GetInSessionAuthService() {
  // The global singleton.
  static raw_ptr<InSessionAuth> in_session_auth_service_;

  if (in_session_auth_service_ == nullptr) {
    in_session_auth_service_ = new InSessionAuth();
  }

  return in_session_auth_service_;
}

}  // namespace

void BindToInSessionAuthService(
    mojo::PendingReceiver<mojom::InSessionAuth> receiver) {
  GetInSessionAuthService()->BindReceiver(std::move(receiver));
}

}  // namespace chromeos::auth
