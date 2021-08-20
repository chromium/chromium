// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_test_manifest_updated_observer.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace web_app {

WebAppTestManifestUpdatedObserver::WebAppTestManifestUpdatedObserver(
    WebAppRegistrar* registrar,
    const std::set<AppId>& listening_for_manifest_updated_app_ids)
    : WebAppTestRegistryObserverAdapter(registrar),
      listening_for_manifest_updated_app_ids_(
          listening_for_manifest_updated_app_ids) {
#if DCHECK_IS_ON()
  for (const AppId& id : listening_for_manifest_updated_app_ids_) {
    DCHECK(!id.empty()) << "Cannot listen for empty ids.";
  }
#endif
}
WebAppTestManifestUpdatedObserver::~WebAppTestManifestUpdatedObserver() =
    default;

AppId WebAppTestManifestUpdatedObserver::Wait() {
  base::RunLoop loop;
  AppId id;
  DCHECK(app_manifest_updated_delegate_.is_null());
  app_manifest_updated_delegate_ =
      base::BindLambdaForTesting([&](const AppId& app_id) {
        id = app_id;
        loop.Quit();
      });
  loop.Run();
  return id;
}

void WebAppTestManifestUpdatedObserver::OnWebAppManifestUpdated(
    const AppId& app_id,
    base::StringPiece old_name) {
  listening_for_manifest_updated_app_ids_.erase(app_id);
  if (!listening_for_manifest_updated_app_ids_.empty())
    return;

  if (app_manifest_updated_delegate_)
    std::move(app_manifest_updated_delegate_).Run(app_id);

  WebAppTestRegistryObserverAdapter::OnWebAppManifestUpdated(app_id, old_name);
}

}  // namespace web_app
