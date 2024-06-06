// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/flags/flags_ui.h"

#include "build/build_config.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/flags_state.h"
#include "content/public/browser/web_ui_message_handler.h"

#ifndef CHROME_BROWSER_UI_WEBUI_FLAGS_FLAGS_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FLAGS_FLAGS_UI_HANDLER_H_

namespace flags_ui {
class FlagsStorage;
}

class FlagsUIHandler : public content::WebUIMessageHandler {
 public:
  FlagsUIHandler();

  FlagsUIHandler(const FlagsUIHandler&) = delete;
  FlagsUIHandler& operator=(const FlagsUIHandler&) = delete;

  ~FlagsUIHandler() override;

  // Initializes the UI handler with the provided flags storage and flags
  // access. If there were flags experiments requested from javascript before
  // this was called, it calls |SendExperimentalFeatures|.
  void Init(std::unique_ptr<flags_ui::FlagsStorage> flags_storage,
            flags_ui::FlagAccess access);

  // Sends experimental features lists to the UI.
  void SendExperimentalFeatures(bool deprecated_features_only);

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Callback for the "requestDeprecatedFeatures" message.
  void HandleRequestDeprecatedFeatures(const base::Value::List& args);

  // Callback for the "requestExperimentFeatures" message.
  void HandleRequestExperimentalFeatures(const base::Value::List& args);

  // Callback for the "enableExperimentalFeature" message.
  void HandleEnableExperimentalFeatureMessage(const base::Value::List& args);

  // Callback for the "setOriginListFlag" message.
  void HandleSetOriginListFlagMessage(const base::Value::List& args);

  // Callback for the "setStringFlag" message.
  void HandleSetStringFlagMessage(const base::Value::List& args);

  // Callback for the "restartBrowser" message. Restores all tabs on restart.
  void HandleRestartBrowser(const base::Value::List& args);

  // Callback for the "resetAllFlags" message.
  void HandleResetAllFlags(const base::Value::List& args);

#if BUILDFLAG(IS_CHROMEOS)
  // Callback for the "CrosUrlFlagsRedirect" message.
  void HandleCrosUrlFlagsRedirect(const base::Value::List& args);
#endif

 private:
  std::unique_ptr<flags_ui::FlagsStorage> flags_storage_;
  flags_ui::FlagAccess access_;
  std::string experimental_features_callback_id_;
  std::string deprecated_features_callback_id_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_FLAGS_FLAGS_UI_HANDLER_H_
