// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/predictors/predictors_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"

using predictors::AutocompleteActionPredictor;
using predictors::ResourcePrefetchPredictor;
using predictors::ResourcePrefetchPredictorTables;

PredictorsHandler::PredictorsHandler(Profile* profile) {
  autocomplete_action_predictor_ =
      predictors::AutocompleteActionPredictorFactory::GetForProfile(profile);
  loading_predictor_ =
      predictors::LoadingPredictorFactory::GetForProfile(profile);
}

PredictorsHandler::~PredictorsHandler() { }

void PredictorsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestAutocompleteActionPredictorDb",
      base::BindRepeating(
          &PredictorsHandler::RequestAutocompleteActionPredictorDb,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestResourcePrefetchPredictorDb",
      base::BindRepeating(
          &PredictorsHandler::RequestResourcePrefetchPredictorDb,
          base::Unretained(this)));
}

void PredictorsHandler::RequestAutocompleteActionPredictorDb(
    const base::Value::List& args) {
  AllowJavascript();
  const bool enabled = !!autocomplete_action_predictor_;
  auto dict = base::Value::Dict().Set("enabled", enabled);
  if (enabled) {
    base::Value::List db;
    for (AutocompleteActionPredictor::DBCacheMap::const_iterator it =
             autocomplete_action_predictor_->db_cache_.begin();
         it != autocomplete_action_predictor_->db_cache_.end();
         ++it) {
      db.Append(
          base::Value::Dict()
              .Set("user_text", it->first.user_text)
              .Set("url", it->first.url.spec())
              .Set("hit_count", it->second.number_of_hits)
              .Set("miss_count", it->second.number_of_misses)
              .Set("confidence", autocomplete_action_predictor_
                                     ->CalculateConfidenceForDbEntry(it)));
    }
    dict.Set("db", std::move(db));
  }

  ResolveJavascriptCallback(args[0] /* callback_id */, dict);
}

void PredictorsHandler::RequestResourcePrefetchPredictorDb(
    const base::Value::List& args) {
  AllowJavascript();
  const bool enabled = (loading_predictor_ != nullptr);
  auto dict = base::Value::Dict().Set("enabled", enabled);

  if (enabled) {
    auto* resource_prefetch_predictor =
        loading_predictor_->resource_prefetch_predictor();
    const bool initialized =
        resource_prefetch_predictor->initialization_state_ ==
        ResourcePrefetchPredictor::INITIALIZED;

    if (initialized) {
      // TODO(alexilin): Add redirects table.

      // Origin table cache.
      base::Value::List db;
      AddOriginDataMapToListValue(
          resource_prefetch_predictor->origin_data_->GetAllCached(), &db);
      dict.Set("origin_db", std::move(db));
    }
  }

  ResolveJavascriptCallback(args[0] /* callback_id */, dict);
}

void PredictorsHandler::AddOriginDataMapToListValue(
    const std::map<std::string, predictors::OriginData>& data_map,
    base::Value::List* db) const {
  for (const auto& p : data_map) {
    auto main = base::Value::Dict().Set("main_frame_host", p.first);
    base::Value::List origins;
    for (const predictors::OriginStat& o : p.second.origins()) {
      origins.Append(
          base::Value::Dict()
              .Set("origin", o.origin())
              .Set("number_of_hits", static_cast<int>(o.number_of_hits()))
              .Set("number_of_misses", static_cast<int>(o.number_of_misses()))
              .Set("consecutive_misses",
                   static_cast<int>(o.consecutive_misses()))
              .Set("position", o.average_position())
              .Set("always_access_network", o.always_access_network())
              .Set("accessed_network", o.accessed_network())
              .Set("score",
                   ResourcePrefetchPredictorTables::ComputeOriginScore(o)));
    }
    main.Set("origins", std::move(origins));
    db->Append(std::move(main));
  }
}
