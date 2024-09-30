// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/reconciling_template_url_data_holder.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"

ReconcilingTemplateURLDataHolder::ReconcilingTemplateURLDataHolder(
    PrefService* pref_service,
    search_engines::SearchEngineChoiceService* search_engine_choice_service)
    : pref_service_{pref_service},
      search_engine_choice_service_{search_engine_choice_service} {}

ReconcilingTemplateURLDataHolder::~ReconcilingTemplateURLDataHolder() = default;

std::pair<std::u16string, bool>
ReconcilingTemplateURLDataHolder::GetOrComputeKeyword() const {
  std::u16string keyword = search_engine_->keyword();

  if (!search_engine_->created_from_play_api) {
    return {std::move(keyword), false};
  }

  // Old Play API 'seznam.cz' and 'seznam.sk' have been consolidated to
  // 'seznam'.
  if (keyword.starts_with(u"seznam.")) {
    return {u"seznam", true};
  }

  // Old Play API 'yahoo.com' entries are reconciled with country-specific
  // definitions.
  if (keyword != u"yahoo.com") {
    return {std::move(keyword), false};
  }

  // The domain name prefix specifies the regional version of Yahoo's search
  // engine requests. Until 08.2024 Android EEA Yahoo keywords all pointed
  // to Yahoo US. See go/chrome:template-url-reconciliation for more
  // information.
  // Extract the Country Code from the Yahoo domain name and use it to
  // construct a keyword that we may find in PrepopulatedEngines.
  GURL yahoo_search_url(search_engine_->url());
  std::string_view yahoo_search_host = yahoo_search_url.host_piece();
  std::string_view country_code =
      yahoo_search_host.substr(0, yahoo_search_host.find('.'));
  keyword = base::UTF8ToUTF16(country_code) + u".yahoo.com";

  return {std::move(keyword), true};
}

std::unique_ptr<TemplateURLData>
ReconcilingTemplateURLDataHolder::FindMatchingBuiltInDefinitionsByKeyword(
    const std::u16string& keyword) const {
  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          pref_service_, search_engine_choice_service_);

  auto engine_iter =
      base::ranges::find(prepopulated_urls, keyword, &TemplateURLData::keyword);

  std::unique_ptr<TemplateURLData> result;
  if (engine_iter != prepopulated_urls.end()) {
    result = std::move(*engine_iter);
  } else if (switches::kReconcileWithAllKnownEngines.Get()) {
    auto all_engines = TemplateURLPrepopulateData::GetAllPrepopulatedEngines();
    for (const auto* engine : all_engines) {
      if (engine->keyword == keyword) {
        result = TemplateURLDataFromPrepopulatedEngine(*engine);
        break;
      }
    }
  }

  return result;
}

std::unique_ptr<TemplateURLData>
ReconcilingTemplateURLDataHolder::FindMatchingBuiltInDefinitionsById(
    int prepopulate_id) const {
  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          pref_service_, search_engine_choice_service_);

  auto engine_iter = base::ranges::find(prepopulated_urls, prepopulate_id,
                                        &TemplateURLData::prepopulate_id);

  std::unique_ptr<TemplateURLData> result;
  if (engine_iter != prepopulated_urls.end()) {
    result = std::move(*engine_iter);
  } else if (switches::kReconcileWithAllKnownEngines.Get()) {
    auto all_engines = TemplateURLPrepopulateData::GetAllPrepopulatedEngines();
    for (const auto* engine : all_engines) {
      if (engine->id == prepopulate_id) {
        result = TemplateURLDataFromPrepopulatedEngine(*engine);
        break;
      }
    }
  }

  return result;
}

void ReconcilingTemplateURLDataHolder::SetAndReconcile(
    std::unique_ptr<TemplateURLData> data) {
  enum class ReconciliationType {
    kNone,
    kByID,
    kByKeyword,
    kByDomainBasedKeyword,
    kMaxValue = kByDomainBasedKeyword
  };

  search_engine_ = std::move(data);
  if (!search_engine_) {
    return;
  }

  // Evaluate whether items should be reconciled.
  // Permit merging Play entries if feature is enabled.
  bool reconcile_by_keyword =
      search_engine_->created_from_play_api &&
      base::FeatureList::IsEnabled(switches::kTemplateUrlReconciliation);

  // Permit merging by Prepopulated ID (except Play entries).
  bool reconcile_by_id =
      search_engine_->prepopulate_id && !search_engine_->created_from_play_api;

  // Don't call GetPrepopulatedEngines() if we don't have anything to reconcile.
  if (!(reconcile_by_id || reconcile_by_keyword)) {
    base::UmaHistogramEnumeration("Omnibox.TemplateUrl.Reconciliation.Type",
                                  ReconciliationType::kNone);
    return;
  }

  std::unique_ptr<TemplateURLData> engine;
  if (reconcile_by_keyword) {
    auto [keyword, is_by_domain_based_keyword] = GetOrComputeKeyword();
    engine = FindMatchingBuiltInDefinitionsByKeyword(keyword);

    base::UmaHistogramBoolean(
        is_by_domain_based_keyword
            ? "Omnibox.TemplateUrl.Reconciliation.ByDomainBasedKeyword.Result"
            : "Omnibox.TemplateUrl.Reconciliation.ByKeyword.Result",
        !!engine);
    base::UmaHistogramEnumeration(
        "Omnibox.TemplateUrl.Reconciliation.Type",
        is_by_domain_based_keyword ? ReconciliationType::kByDomainBasedKeyword
                                   : ReconciliationType::kByKeyword);
  } else if (reconcile_by_id) {
    engine = FindMatchingBuiltInDefinitionsById(search_engine_->prepopulate_id);
    base::UmaHistogramBoolean("Omnibox.TemplateUrl.Reconciliation.ByID.Result",
                              !!engine);
    base::UmaHistogramEnumeration("Omnibox.TemplateUrl.Reconciliation.Type",
                                  ReconciliationType::kByID);
  }

  if (!engine) {
    return;
  }

  if (!search_engine_->safe_for_autoreplace) {
    engine->safe_for_autoreplace = false;
    engine->SetKeyword(search_engine_->keyword());
    engine->SetShortName(search_engine_->short_name());
  }

  engine->id = search_engine_->id;
  engine->sync_guid = search_engine_->sync_guid;
  engine->date_created = search_engine_->date_created;
  engine->last_modified = search_engine_->last_modified;
  engine->last_visited = search_engine_->last_visited;
  engine->favicon_url = search_engine_->favicon_url;
  engine->created_from_play_api = search_engine_->created_from_play_api;

  search_engine_ = std::move(engine);
}
