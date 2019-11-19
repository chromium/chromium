// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBAPKS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WEBAPKS_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/webapk/webapk_handler_delegate.h"
#include "chrome/browser/android/webapk/webapk_info.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}  // namespace base

// Handles JavaScript messages from the chrome://webapks page.
class WebApksHandler : public content::WebUIMessageHandler {
 public:
  WebApksHandler();
  ~WebApksHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // Handler for the "requestWebApksInfo" message. This requests
  // information for the installed WebAPKs and returns it to JS using
  // OnWebApkInfoReceived().
  void HandleRequestWebApksInfo(const base::ListValue* args);

  // Handler for the "requestWebApkUpdate" message. This sets the
  // update flag for a set of WebAPKs. |args| should contain the
  // webapp IDs of the WebAPKs to update.
  void HandleRequestWebApkUpdate(const base::ListValue* args);

 private:
  // Called once for each installed WebAPK when the WebAPK Info is retrieved.
  void OnWebApkInfoRetrieved(const WebApkInfo& webapk_info);

  WebApkHandlerDelegate delegate_;

  DISALLOW_COPY_AND_ASSIGN(WebApksHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBAPKS_HANDLER_H_
