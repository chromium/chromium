// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTERNALS_LENS_LENS_INTERNALS_UI_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_INTERNALS_LENS_LENS_INTERNALS_UI_MESSAGE_HANDLER_H_

#include "base/android/scoped_java_ref.h"
#include "base/scoped_observation.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

class LensInternalsUIMessageHandler : public content::WebUIMessageHandler {
 public:
  explicit LensInternalsUIMessageHandler(Profile* profile);
  ~LensInternalsUIMessageHandler() override;

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  // Logger::Observer implementation.
  void HandleStartDebugMode(const base::Value::List& args);
  void HandleRefreshDebugData(const base::Value::List& args);
  void HandleStopDebugMode(const base::Value::List& args);

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTERNALS_LENS_LENS_INTERNALS_UI_MESSAGE_HANDLER_H_
