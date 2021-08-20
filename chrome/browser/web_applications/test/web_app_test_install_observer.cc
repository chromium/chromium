// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_test_install_observer.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"

namespace web_app {

WebAppTestInstallObserver::WebAppTestInstallObserver(
    Profile* profile,
    const std::set<AppId>& listening_for_install_app_ids)
    : WebAppTestRegistryObserverAdapter(profile),
      listening_for_install_app_ids_(listening_for_install_app_ids) {
#if DCHECK_IS_ON()
  for (const AppId& id : listening_for_install_app_ids_) {
    DCHECK(!id.empty()) << "Cannot listen for empty ids.";
  }
#endif
}
WebAppTestInstallObserver::~WebAppTestInstallObserver() = default;

AppId WebAppTestInstallObserver::Wait() {
  base::RunLoop loop;
  AppId id;
  DCHECK(app_installed_delegate_.is_null());
  app_installed_delegate_ =
      base::BindLambdaForTesting([&](const AppId& app_id) {
        id = app_id;
        loop.Quit();
      });
  loop.Run();
  return id;
}

void WebAppTestInstallObserver::OnWebAppInstalled(const AppId& app_id) {
  listening_for_install_app_ids_.erase(app_id);
  if (!listening_for_install_app_ids_.empty())
    return;

  if (app_installed_delegate_)
    std::move(app_installed_delegate_).Run(app_id);

  WebAppTestRegistryObserverAdapter::OnWebAppInstalled(app_id);
}

}  // namespace web_app
