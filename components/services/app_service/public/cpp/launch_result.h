// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_LAUNCH_RESULT_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_LAUNCH_RESULT_H_

#include "base/functional/callback_forward.h"

namespace apps {
// LaunchResult and LaunchCallback can be used in ChromeOS and other
// desktop platforms. So this struct can't be moved to AppPublisher.

enum class LaunchResult { kSuccess, kFailed, kFailedDirectoryNotShared };

using LaunchCallback = base::OnceCallback<void(LaunchResult)>;

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_LAUNCH_RESULT_H_
