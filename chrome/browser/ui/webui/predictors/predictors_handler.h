// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PREDICTORS_PREDICTORS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PREDICTORS_PREDICTORS_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace predictors {
class AutocompleteActionPredictor;
class LoadingPredictor;
}

class Profile;

// The handler for Javascript messages for about:predictors.
class PredictorsHandler : public content::WebUIMessageHandler {
 public:
  explicit PredictorsHandler(Profile* profile);

  PredictorsHandler(const PredictorsHandler&) = delete;
  PredictorsHandler& operator=(const PredictorsHandler&) = delete;

  ~PredictorsHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  // Synchronously fetches the database from AutocompleteActionPredictor and
  // calls into JS with the resulting `base::Value::Dict`.
  void RequestAutocompleteActionPredictorDb(const base::Value::List& args);

  // Fetches stats for the ResourcePrefetchPredictor and returns it as a
  // `base::Value::Dict` to the JS.
  void RequestResourcePrefetchPredictorDb(const base::Value::List& args);

  // Helpers for RequestResourcePrefetchPredictorDb.
  void AddOriginDataMapToListValue(
      const std::map<std::string, predictors::OriginData>& data_map,
      base::Value::List* db) const;

  raw_ptr<predictors::AutocompleteActionPredictor>
      autocomplete_action_predictor_;
  raw_ptr<predictors::LoadingPredictor> loading_predictor_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PREDICTORS_PREDICTORS_HANDLER_H_
