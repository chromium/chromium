// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"

namespace web_app {

// static
void WebAppLaunchManager::SetOpenApplicationCallbackForTesting(
    OpenApplicationCallback callback) {
  GetOpenApplicationCallbackForTesting() = std::move(callback);
}

// static
WebAppLaunchManager::OpenApplicationCallback&
WebAppLaunchManager::GetOpenApplicationCallbackForTesting() {
  static base::NoDestructor<WebAppLaunchManager::OpenApplicationCallback>
      callback;
  return *callback;
}

}  // namespace web_app
