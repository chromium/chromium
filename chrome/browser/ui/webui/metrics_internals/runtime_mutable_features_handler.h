// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_RUNTIME_MUTABLE_FEATURES_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_RUNTIME_MUTABLE_FEATURES_HANDLER_H_

#include <memory>

#include "base/values.h"
#include "components/metrics/debug/runtime_mutable_features_handler_base.h"
#include "content/public/browser/web_ui_message_handler.h"

// UI Handler for the Runtime-Mutable-Features tab in
// chrome://metrics-internals.
class RuntimeMutableFeaturesHandler
    : public content::WebUIMessageHandler,
      public metrics::RuntimeMutableFeaturesHandlerBase::Delegate {
 public:
  RuntimeMutableFeaturesHandler();

  RuntimeMutableFeaturesHandler(const RuntimeMutableFeaturesHandler&) = delete;
  RuntimeMutableFeaturesHandler& operator=(
      const RuntimeMutableFeaturesHandler&) = delete;

  ~RuntimeMutableFeaturesHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // metrics::RuntimeMutableFeaturesHandlerBase::Delegate:
  void ResolvePageCallback(const base::ValueView callback_id,
                           const base::ValueView response) override;

 private:
  void HandleFetchRuntimeMutableFeatures(const base::ListValue& args);
  void HandleIsSeedFetchingPaused(const base::ListValue& args);
  void HandleSetSeedFetchingPaused(const base::ListValue& args);
  void HandleUploadSeed(const base::ListValue& args);

  std::unique_ptr<metrics::RuntimeMutableFeaturesHandlerBase> base_handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_RUNTIME_MUTABLE_FEATURES_HANDLER_H_
