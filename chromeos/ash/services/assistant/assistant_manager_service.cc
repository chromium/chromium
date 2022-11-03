// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/assistant_manager_service.h"

namespace ash::assistant {

AuthenticationStateObserver::AuthenticationStateObserver() = default;

AuthenticationStateObserver::~AuthenticationStateObserver() = default;

mojo::PendingRemote<libassistant::mojom::AuthenticationStateObserver>
AuthenticationStateObserver::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void AuthenticationStateObserver::ResetAuthenticationStateObserver() {
  receiver_.reset();
}

}  // namespace ash::assistant
