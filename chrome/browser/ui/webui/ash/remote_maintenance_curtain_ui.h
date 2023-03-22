// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_REMOTE_MAINTENANCE_CURTAIN_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_REMOTE_MAINTENANCE_CURTAIN_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace ash {

class RemoteMaintenanceCurtainUI : public content::WebUIController {
 public:
  explicit RemoteMaintenanceCurtainUI(content::WebUI* web_ui);

  RemoteMaintenanceCurtainUI(const RemoteMaintenanceCurtainUI&) = delete;
  RemoteMaintenanceCurtainUI& operator=(const RemoteMaintenanceCurtainUI&) =
      delete;

  ~RemoteMaintenanceCurtainUI() override = default;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_REMOTE_MAINTENANCE_CURTAIN_UI_H_
