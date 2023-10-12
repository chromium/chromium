// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DLP_INTERNALS_DLP_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DLP_INTERNALS_DLP_INTERNALS_UI_H_

#include "ui/webui/mojo_web_ui_controller.h"

namespace policy {

// UI controller for chrome://dlp-internals.
class DlpInternalsUI : public ui::MojoWebUIController {
 public:
  explicit DlpInternalsUI(content::WebUI* web_ui);

  DlpInternalsUI(const DlpInternalsUI&) = delete;
  DlpInternalsUI& operator=(const DlpInternalsUI&) = delete;

  ~DlpInternalsUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace policy

#endif  // CHROME_BROWSER_UI_WEBUI_DLP_INTERNALS_DLP_INTERNALS_UI_H_
