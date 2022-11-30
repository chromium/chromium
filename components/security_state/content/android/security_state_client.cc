// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/content/android/security_state_client.h"

namespace security_state {

static SecurityStateClient* g_client;

void SetSecurityStateClient(SecurityStateClient* client) {
  g_client = client;
}

SecurityStateClient* GetSecurityStateClient() {
  return g_client;
}

std::unique_ptr<SecurityStateModelDelegate>
SecurityStateClient::MaybeCreateSecurityStateModelDelegate() {
  return nullptr;
}

}  // namespace security_state
