// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/reconciling_template_url_data_holder.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_prepopulate_data_resolver.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"

namespace {

ReconcilingTemplateURLDataHolder::ReconciliationVariant
GetReconciliationVariant(
    ReconcilingTemplateURLDataHolder::ReconciliationType type) {
  using ReconciliationType =
      ReconcilingTemplateURLDataHolder::ReconciliationType;
  using ReconciliationVariant =
      ReconcilingTemplateURLDataHolder::ReconciliationVariant;
  switch (type) {
    case ReconciliationType::kByKeyword:
      return ReconciliationVariant::kByKeyword;

    case ReconciliationType::kBySeznamKeyword:
    case ReconciliationType::kByYahooKeyword:
      return ReconciliationVariant::kByDomainBasedKeyword;

    case ReconciliationType::kByIdFromAllEngines:
    case ReconciliationType::kByIdFromRegionalEngines:
    case ReconciliationType::kByIdFallthrough:
    case ReconciliationType::kByMigrateToId:
      return ReconciliationVariant::kByID;

    case ReconciliationType::kNone:
      NOTREACHED();
  }
  NOTREACHED();
}

void RecordOutcomeMetrics(
    ReconcilingTemplateURLDataHolder::ReconciliationType reconciliation_type,
    bool success) {
  using ReconciliationVariant =
      ReconcilingTemplateURLDataHolder::ReconciliationVariant;

  base::UmaHistogramEnumeration("Omnibox.TemplateUrl.Reconciliation.Type",
                                reconciliation_type);
  std::string variant_name;
  switch (GetReconciliationVariant(reconciliation_type)) {
    case ReconciliationVariant::kByID:
      variant_name = "ByID";
      break;
    case ReconciliationVariant::kByKeyword:
      variant_name = "ByKeyword";
      break;
    case ReconciliationVariant::kByDomainBasedKeyword:
      variant_name = "ByDomainBasedKeyword";
      break;
  }
  CHECK(!variant_name.empty());
  base::UmaHistogramBoolean(base::StrCat({"Omnibox.TemplateUrl.Reconciliation.",
                                          variant_name, ".Result"}),
                            success);
}

}  // namespace

ReconcilingTemplateURLDataHolder::ReconcilingTemplateURLDataHolder(
    TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver)
    : prepopulate_data_resolver_(prepopulate_data_resolver) {}

ReconcilingTemplateURLDataHolder::~ReconcilingTemplateURLDataHolder() = default;

std::pair<std::u16string, ReconcilingTemplateURLDataHolder::ReconciliationType>
ReconcilingTemplateURLDataHolder::GetOrComputeKeyword() const {
  std::u16string keyword = search_engine_->keyword();

  if (search_engine_->regulatory_origin == RegulatoryExtensionType::kDefault) {
    return {std::move(keyword), ReconciliationType::kByKeyword};
  }

  // Old Play API 'seznam.cz' and 'seznam.sk' have been consolidated to
  // 'seznam'.
  if (keyword.starts_with(u"seznam.")) {
    return {u"seznam", ReconciliationType::kBySeznamKeyword};
  }

  // Old Play API 'yahoo.com' entries are reconciled with country-specific
  // definitions.
  if (keyword == u"yahoo.com") {
    // The domain name prefix specifies the regional version of Yahoo's search
    // engine requests. Until 08.2024 Android EEA Yahoo keywords all pointed
    // to Yahoo US. See go/chrome:template-url-reconciliation for more
    // information.
    // Extract the Country Code from the Yahoo domain name and use it to
    // construct a keyword that we may find in PrepopulatedEngines.
    GURL yahoo_search_url(search_engine_->url());
    std::string_view yahoo_search_host = yahoo_search_url.host();
    std::string_view country_code =
        yahoo_search_host.substr(0, yahoo_search_host.find('.'));
    keyword = base::UTF8ToUTF16(country_code) + u".yahoo.com";

    return {std::move(keyword), ReconciliationType::kByYahooKeyword};
  }

  return {std::move(keyword), ReconciliationType::kByKeyword};
}

std::unique_ptr<TemplateURLData>
ReconcilingTemplateURLDataHolder::FindMatchingBuiltInDefinitionsByKeyword(
    const std::u16string& keyword) const {
  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls =
      prepopulate_data_resolver_->GetPrepopulatedEngines();

  auto engine_iter =
      std::ranges::find(prepopulated_urls, keyword, &TemplateURLData::keyword);

  std::unique_ptr<TemplateURLData> result;
  if (engine_iter != prepopulated_urls.end()) {
    result = std::move(*engine_iter);
  } else {
    // Search the entire search engine database to find matching entry.
    auto all_engines = regional_capabilities::GetAllPrepopulatedEngines();
    for (const auto* engine : all_engines) {
      if (engine->keyword == keyword) {
        result = TemplateURLDataFromPrepopulatedEngine(*engine);
        break;
      }
    }
  }

  return result;
}

std::pair<std::unique_ptr<TemplateURLData>,
          ReconcilingTemplateURLDataHolder::ReconciliationType>
ReconcilingTemplateURLDataHolder::FindMatchingBuiltInDefinitionsById(
    const TemplateURLData& data_to_match) const {
  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls =
      prepopulate_data_resolver_->GetPrepopulatedEngines();

  if (auto engine_iter =
          std::ranges::find(prepopulated_urls, data_to_match.prepopulate_id,
                            &TemplateURLData::prepopulate_id);
      engine_iter != prepopulated_urls.end()) {
    return {std::move(*engine_iter),
            ReconciliationType::kByIdFromRegionalEngines};
  }

  // Search for potential migrations
  if (std::unique_ptr<TemplateURLData> engine =
          prepopulate_data_resolver_->TryGetMigratedEngine(data_to_match);
      engine != nullptr) {
    return {std::move(engine), ReconciliationType::kByMigrateToId};
  }

  // Search the entire search engine database to find matching entry.
  auto all_engines = regional_capabilities::GetAllPrepopulatedEngines();
  for (const auto* engine : all_engines) {
    if (engine->id == data_to_match.prepopulate_id) {
      return {TemplateURLDataFromPrepopulatedEngine(*engine),
              ReconciliationType::kByIdFromAllEngines};
    }
  }

  return {nullptr, ReconciliationType::kByIdFallthrough};
}

void ReconcilingTemplateURLDataHolder::SetAndReconcile(
    std::unique_ptr<TemplateURLData> data) {
  search_engine_ = std::move(data);
  if (!search_engine_) {
    return;
  }

  // Evaluate whether items should be reconciled.
  // Permit merging regulatory entries if feature is enabled.
  bool reconcile_by_keyword =
      search_engine_->regulatory_origin != RegulatoryExtensionType::kDefault;

  // Permit merging by Prepopulated ID (except regulatory entries).
  bool reconcile_by_id =
      search_engine_->prepopulate_id &&
      (search_engine_->regulatory_origin == RegulatoryExtensionType::kDefault);

  // Don't call GetPrepopulatedEngines() if we don't have anything to reconcile.
  if (!(reconcile_by_id || reconcile_by_keyword)) {
    base::UmaHistogramEnumeration("Omnibox.TemplateUrl.Reconciliation.Type",
                                  ReconciliationType::kNone);
    return;
  }

  std::unique_ptr<TemplateURLData> engine;
  ReconciliationType reconciliation_type;
  if (reconcile_by_keyword) {
    std::u16string keyword;
    std::tie(keyword, reconciliation_type) = GetOrComputeKeyword();
    engine = FindMatchingBuiltInDefinitionsByKeyword(keyword);
  } else {
    CHECK(reconcile_by_id);
    std::tie(engine, reconciliation_type) =
        FindMatchingBuiltInDefinitionsById(*search_engine_.get());
  }

  CHECK_NE(reconciliation_type, ReconciliationType::kNone);
  RecordOutcomeMetrics(reconciliation_type, /* success= */ !!engine);

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
  engine->regulatory_origin = search_engine_->regulatory_origin;

  search_engine_ = std::move(engine);
}
