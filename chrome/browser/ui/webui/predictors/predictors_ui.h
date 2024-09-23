// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PREDICTORS_PREDICTORS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PREDICTORS_PREDICTORS_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class PredictorsUI;

class PredictorsUIConfig : public content::DefaultWebUIConfig<PredictorsUI> {
 public:
  PredictorsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIPredictorsHost) {}
};

class PredictorsUI : public content::WebUIController {
 public:
  explicit PredictorsUI(content::WebUI* web_ui);

  PredictorsUI(const PredictorsUI&) = delete;
  PredictorsUI& operator=(const PredictorsUI&) = delete;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PREDICTORS_PREDICTORS_UI_H_
