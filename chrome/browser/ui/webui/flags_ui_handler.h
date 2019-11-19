// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/flags_ui.h"

#include "build/build_config.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/flags_state.h"
#include "content/public/browser/web_ui_message_handler.h"

#ifndef CHROME_BROWSER_UI_WEBUI_FLAGS_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FLAGS_UI_HANDLER_H_

namespace flags_ui {
class FlagsStorage;
}

class FlagsUIHandler : public content::WebUIMessageHandler {
 public:
  FlagsUIHandler();
  ~FlagsUIHandler() override;

  // Initializes the UI handler with the provided flags storage and flags
  // access. If there were flags experiments requested from javascript before
  // this was called, it calls |HandleRequestExperimentalFeatures| again.
  void Init(flags_ui::FlagsStorage* flags_storage, flags_ui::FlagAccess access);

  // Configures the handler to return either all features or deprecated
  // features only.
  void set_deprecated_features_only(bool deprecatedFeaturesOnly) {
    deprecated_features_only_ = deprecatedFeaturesOnly;
  }

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Callback for the "requestExperimentFeatures" message.
  void HandleRequestExperimentalFeatures(const base::ListValue* args);

  // Callback for the "enableExperimentalFeature" message.
  void HandleEnableExperimentalFeatureMessage(const base::ListValue* args);

  // Callback for the "setOriginListFlag" message.
  void HandleSetOriginListFlagMessage(const base::ListValue* args);

  // Callback for the "restartBrowser" message. Restores all tabs on restart.
  void HandleRestartBrowser(const base::ListValue* args);

  // Callback for the "resetAllFlags" message.
  void HandleResetAllFlags(const base::ListValue* args);

 private:
  std::unique_ptr<flags_ui::FlagsStorage> flags_storage_;
  flags_ui::FlagAccess access_;
  bool experimental_features_requested_;
  bool deprecated_features_only_;

  DISALLOW_COPY_AND_ASSIGN(FlagsUIHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_FLAGS_UI_HANDLER_H_
