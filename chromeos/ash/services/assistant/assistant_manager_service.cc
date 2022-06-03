// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/assistant_manager_service.h"

namespace chromeos {
namespace assistant {

AuthenticationStateObserver::AuthenticationStateObserver() = default;

AuthenticationStateObserver::~AuthenticationStateObserver() = default;

mojo::PendingRemote<
    ::chromeos::libassistant::mojom::AuthenticationStateObserver>
AuthenticationStateObserver::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

}  // namespace assistant
}  // namespace chromeos
