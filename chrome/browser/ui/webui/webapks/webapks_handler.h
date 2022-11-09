// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBAPKS_WEBAPKS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WEBAPKS_WEBAPKS_HANDLER_H_

#include "chrome/browser/android/webapk/webapk_handler_delegate.h"
#include "chrome/browser/android/webapk/webapk_info.h"
#include "content/public/browser/web_ui_message_handler.h"

// Handles JavaScript messages from the chrome://webapks page.
class WebApksHandler : public content::WebUIMessageHandler {
 public:
  WebApksHandler();

  WebApksHandler(const WebApksHandler&) = delete;
  WebApksHandler& operator=(const WebApksHandler&) = delete;

  ~WebApksHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // Handler for the "requestWebApksInfo" message. This requests
  // information for the installed WebAPKs and returns it to JS using
  // OnWebApkInfoReceived().
  void HandleRequestWebApksInfo(const base::Value::List& args);

  // Handler for the "requestWebApkUpdate" message. This sets the
  // update flag for a set of WebAPKs. |args| should contain the
  // webapp IDs of the WebAPKs to update.
  void HandleRequestWebApkUpdate(const base::Value::List& args);

 private:
  // Called once for each installed WebAPK when the WebAPK Info is retrieved.
  void OnWebApkInfoRetrieved(const WebApkInfo& webapk_info);

  WebApkHandlerDelegate delegate_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBAPKS_WEBAPKS_HANDLER_H_
