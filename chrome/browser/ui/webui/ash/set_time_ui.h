// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SET_TIME_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SET_TIME_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash {

class SetTimeUI;

// WebUIConfig for chrome://set-time
class SetTimeUIConfig : public content::DefaultWebUIConfig<SetTimeUI> {
 public:
  SetTimeUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISetTimeHost) {}
};

// The WebUI for chrome://set-time.
class SetTimeUI : public ui::WebDialogUI {
 public:
  explicit SetTimeUI(content::WebUI* web_ui);

  SetTimeUI(const SetTimeUI&) = delete;
  SetTimeUI& operator=(const SetTimeUI&) = delete;

  ~SetTimeUI() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SET_TIME_UI_H_
