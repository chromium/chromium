// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_SHORTCUT_HANDLING_SUB_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_SHORTCUT_HANDLING_SUB_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

class WebAppIconManager;
class WebAppRegistrar;

class ShortcutHandlingSubManager : public OsIntegrationSubManager {
 public:
  ShortcutHandlingSubManager(WebAppIconManager& icon_manager,
                             WebAppRegistrar& registrar);
  ~ShortcutHandlingSubManager() override;
  void Start() override;
  void Shutdown() override;
  void Configure(const AppId& app_id,
                 proto::WebAppOsIntegrationState& desired_state,
                 base::OnceClosure configure_done) override;
  void Execute(
      const AppId& app_id,
      const proto::WebAppOsIntegrationState& desired_state,
      const absl::optional<proto::WebAppOsIntegrationState>& current_state,
      base::OnceClosure callback) override;

 private:
  void StoreIconDataFromDisk(proto::ShortcutState* shortcut_states,
                             base::OnceClosure configure_done,
                             base::flat_map<SquareSizePx, base::Time> time_map);

  const raw_ref<WebAppIconManager> icon_manager_;
  const raw_ref<WebAppRegistrar> registrar_;

  base::WeakPtrFactory<ShortcutHandlingSubManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_SHORTCUT_HANDLING_SUB_MANAGER_H_
