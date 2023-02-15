// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"

#include <string>
#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_launch_process.h"
#include "chrome/browser/web_applications/web_app_provider.h"

namespace web_app {

WebAppLaunchManager::WebAppLaunchManager(Profile* profile)
    : profile_(profile),
      provider_(WebAppProvider::GetForLocalAppsUnchecked(profile)) {}

WebAppLaunchManager::~WebAppLaunchManager() = default;

content::WebContents* WebAppLaunchManager::OpenApplication(
    apps::AppLaunchParams&& params) {
  if (GetOpenApplicationCallbackForTesting())
    return GetOpenApplicationCallbackForTesting().Run(std::move(params));

  WebAppProvider* provider =
      WebAppProvider::GetForLocalAppsUnchecked(profile_.get());
  DCHECK(provider);
  return WebAppLaunchProcess::CreateAndRun(
      *profile_, provider->registrar_unsafe(),
      provider->os_integration_manager(), params);
}

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
