// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_SHORTCUT_SUB_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_SHORTCUT_SUB_MANAGER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace web_app {

class WebAppProvider;
struct ShortcutInfo;
struct SynchronizeOsOptions;

class ShortcutSubManager : public OsIntegrationSubManager {
 public:
  ShortcutSubManager(Profile& profile, WebAppProvider& provider);
  ~ShortcutSubManager() override;
  void Configure(const webapps::AppId& app_id,
                 proto::WebAppOsIntegrationState& desired_state,
                 base::OnceClosure configure_done) override;
  void Execute(const webapps::AppId& app_id,
               const std::optional<SynchronizeOsOptions>& synchronize_options,
               const proto::WebAppOsIntegrationState& desired_state,
               const proto::WebAppOsIntegrationState& current_state,
               base::OnceClosure callback) override;
  void ForceUnregister(const webapps::AppId& app_id,
                       base::OnceClosure callback) override;

 private:
  void CreateShortcut(const webapps::AppId& app_id,
                      std::optional<SynchronizeOsOptions> synchronize_options,
                      base::OnceClosure on_complete,
                      std::unique_ptr<ShortcutInfo> shortcut_info);
  void UpdateShortcut(const webapps::AppId& app_id,
                      std::optional<SynchronizeOsOptions> synchronize_options,
                      const std::u16string& old_app_title,
                      base::OnceClosure on_complete,
                      std::unique_ptr<ShortcutInfo> shortcut_info);
  void OnShortcutsDeleted(const webapps::AppId& app_id,
                          base::OnceClosure final_callback,
                          bool success);
  void StoreIconDataFromDisk(proto::ShortcutDescription* shortcut,
                             base::flat_map<SquareSizePx, base::Time> time_map);

  const raw_ref<Profile> profile_;
  const raw_ref<WebAppProvider> provider_;

  base::WeakPtrFactory<ShortcutSubManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_SHORTCUT_SUB_MANAGER_H_
