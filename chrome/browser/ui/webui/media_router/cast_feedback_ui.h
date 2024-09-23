// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_CAST_FEEDBACK_UI_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_CAST_FEEDBACK_UI_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

class Profile;

namespace content {
class WebContents;
class WebUI;
}  // namespace content

namespace media_router {

// The main object controlling the Cast feedback
// (chrome://cast-feedback) page.
class CastFeedbackUI : public content::WebUIController {
 public:
  explicit CastFeedbackUI(content::WebUI* web_ui);
  ~CastFeedbackUI() override;

 private:
  void OnCloseMessage(const base::Value::List&);

  const raw_ptr<Profile> profile_;
  const raw_ptr<content::WebContents> web_contents_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// WebUIConfig for chrome://cast-feedback
class CastFeedbackUIConfig
    : public content::DefaultWebUIConfig<CastFeedbackUI> {
 public:
  CastFeedbackUIConfig();

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_CAST_FEEDBACK_UI_H_
