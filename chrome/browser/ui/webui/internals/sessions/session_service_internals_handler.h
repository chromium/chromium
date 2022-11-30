// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTERNALS_SESSIONS_SESSION_SERVICE_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_INTERNALS_SESSIONS_SESSION_SERVICE_INTERNALS_HANDLER_H_

#include <string>

#include "content/public/browser/web_ui_data_source.h"

class Profile;

// Provides functions for showing a diagnostic page for the SessionService.
class SessionServiceInternalsHandler {
 public:
  SessionServiceInternalsHandler() = delete;
  SessionServiceInternalsHandler(const SessionServiceInternalsHandler&) =
      delete;
  SessionServiceInternalsHandler& operator=(
      const SessionServiceInternalsHandler&) = delete;
  ~SessionServiceInternalsHandler() = delete;

  // These functions are expected to be called from
  // WebUIDataSource::SetRequestFilter() callbacks.
  static bool ShouldHandleWebUIRequestCallback(const std::string& path);
  static void HandleWebUIRequestCallback(
      Profile* profile,
      const std::string& path,
      content::WebUIDataSource::GotDataCallback callback);
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTERNALS_SESSIONS_SESSION_SERVICE_INTERNALS_HANDLER_H_
