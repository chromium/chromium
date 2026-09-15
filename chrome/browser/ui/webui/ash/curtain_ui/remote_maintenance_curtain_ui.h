// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CURTAIN_UI_REMOTE_MAINTENANCE_CURTAIN_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CURTAIN_UI_REMOTE_MAINTENANCE_CURTAIN_UI_H_

#include "ash/constants/webui_url_constants.h"
#include "base/memory/raw_ref.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/mojo_web_ui_controller.h"

class PrefService;

namespace ash {

class RemoteMaintenanceCurtainUI;

class RemoteMaintenanceCurtainUIConfig : public content::WebUIConfig {
 public:
  // `local_state` must be non-null and must outlive `this`.
  explicit RemoteMaintenanceCurtainUIConfig(PrefService* local_state);
  RemoteMaintenanceCurtainUIConfig(const RemoteMaintenanceCurtainUIConfig&) =
      delete;
  RemoteMaintenanceCurtainUIConfig& operator=(
      const RemoteMaintenanceCurtainUIConfig&) = delete;
  ~RemoteMaintenanceCurtainUIConfig() override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;

 private:
  const raw_ref<PrefService> local_state_;
};

class RemoteMaintenanceCurtainUI : public ui::MojoWebUIController {
 public:
  RemoteMaintenanceCurtainUI(const PrefService& local_state,
                             content::WebUI* web_ui);

  RemoteMaintenanceCurtainUI(const RemoteMaintenanceCurtainUI&) = delete;
  RemoteMaintenanceCurtainUI& operator=(const RemoteMaintenanceCurtainUI&) =
      delete;

  ~RemoteMaintenanceCurtainUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CURTAIN_UI_REMOTE_MAINTENANCE_CURTAIN_UI_H_
