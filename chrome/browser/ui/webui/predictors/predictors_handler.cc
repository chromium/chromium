// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/predictors/predictors_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
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
    const base::ListValue* args) {
  AllowJavascript();
  const bool enabled = !!autocomplete_action_predictor_;
  base::DictionaryValue dict;
  dict.SetBoolean("enabled", enabled);
  if (enabled) {
    auto db = std::make_unique<base::ListValue>();
    for (AutocompleteActionPredictor::DBCacheMap::const_iterator it =
             autocomplete_action_predictor_->db_cache_.begin();
         it != autocomplete_action_predictor_->db_cache_.end();
         ++it) {
      std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue());
      entry->SetString("user_text", it->first.user_text);
      entry->SetString("url", it->first.url.spec());
      entry->SetInteger("hit_count", it->second.number_of_hits);
      entry->SetInteger("miss_count", it->second.number_of_misses);
      entry->SetDouble("confidence",
          autocomplete_action_predictor_->CalculateConfidenceForDbEntry(it));
      db->Append(std::move(entry));
    }
    dict.Set("db", std::move(db));
  }

  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */, dict);
}

void PredictorsHandler::RequestResourcePrefetchPredictorDb(
    const base::ListValue* args) {
  AllowJavascript();
  const bool enabled = (loading_predictor_ != nullptr);
  base::DictionaryValue dict;
  dict.SetBoolean("enabled", enabled);

  if (enabled) {
    auto* resource_prefetch_predictor =
        loading_predictor_->resource_prefetch_predictor();
    const bool initialized =
        resource_prefetch_predictor->initialization_state_ ==
        ResourcePrefetchPredictor::INITIALIZED;

    if (initialized) {
      // TODO(alexilin): Add redirects table.

      // Origin table cache.
      auto db = std::make_unique<base::ListValue>();
      AddOriginDataMapToListValue(
          resource_prefetch_predictor->origin_data_->GetAllCached(), db.get());
      dict.Set("origin_db", std::move(db));
    }
  }

  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */, dict);
}

void PredictorsHandler::AddOriginDataMapToListValue(
    const std::map<std::string, predictors::OriginData>& data_map,
    base::ListValue* db) const {
  for (const auto& p : data_map) {
    auto main = std::make_unique<base::DictionaryValue>();
    main->SetString("main_frame_host", p.first);
    auto origins = std::make_unique<base::ListValue>();
    for (const predictors::OriginStat& o : p.second.origins()) {
      auto origin = std::make_unique<base::DictionaryValue>();
      origin->SetString("origin", o.origin());
      origin->SetInteger("number_of_hits", o.number_of_hits());
      origin->SetInteger("number_of_misses", o.number_of_misses());
      origin->SetInteger("consecutive_misses", o.consecutive_misses());
      origin->SetDouble("position", o.average_position());
      origin->SetBoolean("always_access_network", o.always_access_network());
      origin->SetBoolean("accessed_network", o.accessed_network());
      origin->SetDouble("score",
                        ResourcePrefetchPredictorTables::ComputeOriginScore(o));
      origins->Append(std::move(origin));
    }
    main->Set("origins", std::move(origins));
    db->Append(std::move(main));
  }
}
