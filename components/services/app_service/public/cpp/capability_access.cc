// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/capability_access.h"

namespace apps {

CapabilityAccess::CapabilityAccess(const std::string& app_id)
    : app_id(app_id) {}

CapabilityAccess::~CapabilityAccess() = default;

CapabilityAccessPtr CapabilityAccess::Clone() const {
  auto capability_access = std::make_unique<CapabilityAccess>(app_id);

  capability_access->camera = camera;
  capability_access->microphone = microphone;
  return capability_access;
}

}  // namespace apps
