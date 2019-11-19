// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/client_filterable_state.h"

#include "build/build_config.h"

namespace variations {

// static
Study::Platform ClientFilterableState::GetCurrentPlatform() {
#if defined(OS_WIN)
  return Study::PLATFORM_WINDOWS;
#elif defined(OS_IOS)
  return Study::PLATFORM_IOS;
#elif defined(OS_MACOSX)
  return Study::PLATFORM_MAC;
#elif defined(OS_CHROMEOS)
  return Study::PLATFORM_CHROMEOS;
#elif defined(OS_ANDROID)
  return Study::PLATFORM_ANDROID;
#elif defined(OS_FUCHSIA)
  return Study::PLATFORM_FUCHSIA;
#elif defined(OS_LINUX) || defined(OS_BSD) || defined(OS_SOLARIS)
  // Default BSD and SOLARIS to Linux to not break those builds, although these
  // platforms are not officially supported by Chrome.
  return Study::PLATFORM_LINUX;
#else
#error Unknown platform
#endif
}

ClientFilterableState::ClientFilterableState(
    IsEnterpriseFunction is_enterprise_function)
    : is_enterprise_function_(std::move(is_enterprise_function)) {}
ClientFilterableState::~ClientFilterableState() {}

bool ClientFilterableState::IsEnterprise() const {
  if (!is_enterprise_.has_value())
    is_enterprise_ = std::move(is_enterprise_function_).Run();
  return is_enterprise_.value();
}

}  // namespace variations
