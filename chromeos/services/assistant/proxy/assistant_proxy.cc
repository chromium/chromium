// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/proxy/assistant_proxy.h"

#include "base/check.h"
#include "chromeos/services/assistant/proxy/service_controller.h"

namespace chromeos {
namespace assistant {

AssistantProxy::AssistantProxy()
    : service_controller_(std::make_unique<ServiceController>()) {}

AssistantProxy::~AssistantProxy() = default;

ServiceController& AssistantProxy::service_controller() {
  DCHECK(service_controller_);
  return *service_controller_;
}

}  // namespace assistant
}  // namespace chromeos
