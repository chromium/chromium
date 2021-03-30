// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SESSIONS_SESSION_SERVICE_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SESSIONS_SESSION_SERVICE_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"

// WebUIController that is used to show a diagnostic page for the
// SessionService.
class SessionServiceInternalsUI : public content::WebUIController {
 public:
  explicit SessionServiceInternalsUI(content::WebUI* web_ui);
  SessionServiceInternalsUI(const SessionServiceInternalsUI&) = delete;
  SessionServiceInternalsUI& operator=(const SessionServiceInternalsUI&) =
      delete;
  ~SessionServiceInternalsUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SESSIONS_SESSION_SERVICE_INTERNALS_UI_H_
