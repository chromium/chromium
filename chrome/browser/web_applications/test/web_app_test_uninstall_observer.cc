// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_test_uninstall_observer.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"

namespace web_app {

WebAppTestUninstallObserver::WebAppTestUninstallObserver(
    Profile* profile,
    const std::set<AppId>& listening_for_uninstall_app_ids)
    : WebAppTestRegistryObserverAdapter(profile),
      listening_for_uninstall_app_ids_(listening_for_uninstall_app_ids) {
#if DCHECK_IS_ON()
  for (const AppId& id : listening_for_uninstall_app_ids_) {
    DCHECK(!id.empty()) << "Cannot listen for empty ids.";
  }
#endif
}

WebAppTestUninstallObserver::~WebAppTestUninstallObserver() = default;

AppId WebAppTestUninstallObserver::Wait() {
  base::RunLoop loop;
  AppId id;
  DCHECK(app_uninstalled_delegate_.is_null());
  app_uninstalled_delegate_ =
      base::BindLambdaForTesting([&](const AppId& app_id) {
        id = app_id;
        loop.Quit();
      });
  loop.Run();
  return id;
}

void WebAppTestUninstallObserver::OnWebAppUninstalled(const AppId& app_id) {
  listening_for_uninstall_app_ids_.erase(app_id);
  if (!listening_for_uninstall_app_ids_.empty())
    return;

  if (app_uninstalled_delegate_)
    std::move(app_uninstalled_delegate_).Run(app_id);

  WebAppTestRegistryObserverAdapter::OnWebAppUninstalled(app_id);
}

}  // namespace web_app
