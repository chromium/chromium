// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_MANIFEST_UPDATED_OBSERVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_MANIFEST_UPDATED_OBSERVER_H_

#include <set>

#include "base/callback.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/test/web_app_test_registry_observer_adapter.h"

namespace web_app {

class WebAppRegistrar;

class WebAppTestManifestUpdatedObserver final
    : public WebAppTestRegistryObserverAdapter {
 public:
  explicit WebAppTestManifestUpdatedObserver(
      WebAppRegistrar* registrar,
      const std::set<AppId>& listening_for_manifest_updated_app_ids = {});
  ~WebAppTestManifestUpdatedObserver() final;
  AppId Wait();

  // AppRegistrarObserver:
  void OnWebAppManifestUpdated(const AppId& app_id,
                               base::StringPiece old_name) final;

 private:
  std::set<AppId> listening_for_manifest_updated_app_ids_;
  base::OnceCallback<void(const AppId& app_id)> app_manifest_updated_delegate_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_MANIFEST_UPDATED_OBSERVER_H_
