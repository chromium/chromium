// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/prediction_service_common.h"

#include "build/build_config.h"

namespace permissions {
ClientFeatures_Platform GetCurrentPlatformProto() {
#if defined(OS_WIN)
  return permissions::ClientFeatures_Platform_PLATFORM_WINDOWS;
#elif defined(OS_LINUX)
  return permissions::ClientFeatures_Platform_PLATFORM_LINUX;
#elif defined(OS_ANDROID)
  return permissions::ClientFeatures_Platform_PLATFORM_ANDROID;
#elif defined(OS_MAC)
  return permissions::ClientFeatures_Platform_PLATFORM_MAC_OS;
#else
  return permissions::ClientFeatures_Platform_PLATFORM_UNKNOWN;
#endif
}

}  // namespace permissions
