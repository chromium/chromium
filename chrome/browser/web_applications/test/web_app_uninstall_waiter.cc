// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_uninstall_waiter.h"

#include "chrome/browser/web_applications/components/web_app_provider_base.h"

namespace web_app {

WebAppUninstallWaiter::WebAppUninstallWaiter(Profile* profile, AppId app_id)
    : app_id_(std::move(app_id)) {
  observer_.Add(&WebAppProviderBase::GetProviderBase(profile)->registrar());
}
WebAppUninstallWaiter::~WebAppUninstallWaiter() = default;

void WebAppUninstallWaiter::Wait() {
  run_loop_.Run();
}

void WebAppUninstallWaiter::OnWebAppUninstalled(const AppId& app_id) {
  if (app_id == app_id_)
    run_loop_.Quit();
}

}  // namespace web_app
