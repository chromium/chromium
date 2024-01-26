// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_CAPABILITY_ACCESS_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_CAPABILITY_ACCESS_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/component_export.h"

namespace apps {

// Information about whether an app is accessing some capability, e.g. camera,
// microphone.
struct COMPONENT_EXPORT(APP_TYPES) CapabilityAccess {
  explicit CapabilityAccess(const std::string& app_id);

  CapabilityAccess(const CapabilityAccess&) = delete;
  CapabilityAccess& operator=(const CapabilityAccess&) = delete;

  ~CapabilityAccess();

  std::unique_ptr<CapabilityAccess> Clone() const;

  std::string app_id;

  // Whether the app is accessing camera.
  std::optional<bool> camera;

  // Whether the app is accessing microphone.
  std::optional<bool> microphone;

  // When adding new fields, also update the Merge method and other helpers in
  // components/services/app_service/public/cpp/CapabilityAccessUpdate.*
};

using CapabilityAccessPtr = std::unique_ptr<CapabilityAccess>;

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_CAPABILITY_ACCESS_H_
