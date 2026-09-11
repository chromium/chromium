// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SET_TIME_SET_TIME_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SET_TIME_SET_TIME_UI_H_

#include <memory>

#include "ash/constants/webui_url_constants.h"
#include "base/memory/raw_ref.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/mojo_web_ui_controller.h"

class PrefService;

namespace ash {

class SetTimeUI;

// WebUIConfig for chrome://set-time
class SetTimeUIConfig : public content::WebUIConfig {
 public:
  // `local_state` must be non-null and must outlive `this`.
  explicit SetTimeUIConfig(PrefService* local_state);

  SetTimeUIConfig(const SetTimeUIConfig&) = delete;
  SetTimeUIConfig& operator=(const SetTimeUIConfig&) = delete;

  ~SetTimeUIConfig() override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;

 private:
  const raw_ref<PrefService> local_state_;
};

// The WebUI for chrome://set-time.
class SetTimeUI : public ui::MojoWebDialogUI {
 public:
  // `local_state` must be non-null and must outlive `web_ui`.
  SetTimeUI(PrefService* local_state, content::WebUI* web_ui);

  SetTimeUI(const SetTimeUI&) = delete;
  SetTimeUI& operator=(const SetTimeUI&) = delete;

  ~SetTimeUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SET_TIME_SET_TIME_UI_H_
