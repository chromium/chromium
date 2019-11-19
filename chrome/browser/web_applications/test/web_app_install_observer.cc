// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_install_observer.h"

#include "chrome/browser/web_applications/components/web_app_provider_base.h"

namespace web_app {

WebAppInstallObserver::WebAppInstallObserver(AppRegistrar* registrar) {
  observer_.Add(registrar);
}

WebAppInstallObserver::WebAppInstallObserver(Profile* profile)
    : WebAppInstallObserver(
          &WebAppProviderBase::GetProviderBase(profile)->registrar()) {}

WebAppInstallObserver::~WebAppInstallObserver() = default;

AppId WebAppInstallObserver::AwaitNextInstall() {
  run_loop_.Run();
  return std::move(app_id_);
}

void WebAppInstallObserver::SetWebAppUninstalledDelegate(
    WebAppUninstalledDelegate delegate) {
  app_uninstalled_delegate_ = delegate;
}

void WebAppInstallObserver::OnWebAppInstalled(const AppId& app_id) {
  app_id_ = app_id;
  run_loop_.Quit();
}

void WebAppInstallObserver::OnWebAppUninstalled(const AppId& app_id) {
  if (app_uninstalled_delegate_)
    app_uninstalled_delegate_.Run(app_id);
}

}  // namespace web_app
