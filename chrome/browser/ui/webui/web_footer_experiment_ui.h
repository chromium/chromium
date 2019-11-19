// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEB_FOOTER_EXPERIMENT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WEB_FOOTER_EXPERIMENT_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

class WebFooterExperimentUI : public content::WebUIController {
 public:
  explicit WebFooterExperimentUI(content::WebUI* web_ui);
  ~WebFooterExperimentUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebFooterExperimentUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEB_FOOTER_EXPERIMENT_UI_H_
