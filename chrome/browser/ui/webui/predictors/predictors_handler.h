// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PREDICTORS_PREDICTORS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PREDICTORS_PREDICTORS_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

namespace predictors {
class AutocompleteActionPredictor;
class LoadingPredictor;
}

class Profile;

// The handler for Javascript messages for about:predictors.
class PredictorsHandler : public content::WebUIMessageHandler {
 public:
  explicit PredictorsHandler(Profile* profile);
  ~PredictorsHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  // Synchronously fetches the database from AutocompleteActionPredictor and
  // calls into JS with the resulting DictionaryValue.
  void RequestAutocompleteActionPredictorDb(const base::ListValue* args);

  // Fetches stats for the ResourcePrefetchPredictor and returns it as a
  // DictionaryValue to the JS.
  void RequestResourcePrefetchPredictorDb(const base::ListValue* args);

  // Helpers for RequestResourcePrefetchPredictorDb.
  void AddOriginDataMapToListValue(
      const std::map<std::string, predictors::OriginData>& data_map,
      base::ListValue* db) const;

  predictors::AutocompleteActionPredictor* autocomplete_action_predictor_;
  predictors::LoadingPredictor* loading_predictor_;

  DISALLOW_COPY_AND_ASSIGN(PredictorsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PREDICTORS_PREDICTORS_HANDLER_H_
