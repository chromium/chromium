// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_RUNTIME_MUTABLE_FEATURES_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_RUNTIME_MUTABLE_FEATURES_HANDLER_H_

#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

// UI Handler for the Runtime-Mutable-Features tab in
// chrome://metrics-internals.
class RuntimeMutableFeaturesHandler : public content::WebUIMessageHandler {
 public:
  RuntimeMutableFeaturesHandler();

  RuntimeMutableFeaturesHandler(const RuntimeMutableFeaturesHandler&) = delete;
  RuntimeMutableFeaturesHandler& operator=(
      const RuntimeMutableFeaturesHandler&) = delete;

  ~RuntimeMutableFeaturesHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleFetchRuntimeMutableFeatures(const base::ListValue& args);
  void HandleIsSeedFetchingPaused(const base::ListValue& args);
  void HandleSetSeedFetchingPaused(const base::ListValue& args);
  void HandleUploadSeed(const base::ListValue& args);
};

#endif  // CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_RUNTIME_MUTABLE_FEATURES_HANDLER_H_
