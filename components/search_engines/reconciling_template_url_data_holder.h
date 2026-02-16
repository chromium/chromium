// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_RECONCILING_TEMPLATE_URL_DATA_HOLDER_H_
#define COMPONENTS_SEARCH_ENGINES_RECONCILING_TEMPLATE_URL_DATA_HOLDER_H_

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data_resolver.h"

// TemplateURLData holder that reconciles (where appropriate) supplied Search
// Engine definitions with Chrome built-in definitions.
// Reconciliation aims to fill gaps in partial TemplateURLData definitions.
class ReconcilingTemplateURLDataHolder {
 public:
  explicit ReconcilingTemplateURLDataHolder(
      TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver);

  ~ReconcilingTemplateURLDataHolder();

  // Stores the supplied Search Engine definition. Attempts to reconcile
  // supplied definition with Chrome built-in definitions.
  void SetAndReconcile(std::unique_ptr<TemplateURLData> data);

  // Testing call that bypasses all merge logic.
  void SetSearchEngineBypassingReconciliationForTesting(
      std::unique_ptr<TemplateURLData> data) {
    search_engine_ = std::move(data);
  }

  // Retrieves the currently set Search Engine definition.
  // The definition reported by this call may differ from the definition
  // supplied via Set() if the internal logic determined the possibility to
  // reconcile it with Chrome prepopulated engines.
  const TemplateURLData* Get() const { return search_engine_.get(); }

  // LINT.IfChange(ReconciliationType)
  enum class ReconciliationType {
    kNone = 0,
    // kByID = 1, // Deprecated
    kByKeyword = 2,
    // kByDomainBasedKeyword = 3, // Deprecated
    kBySeznamKeyword = 4,
    kByYahooKeyword = 5,
    kByIdFromAllEngines = 6,
    kByIdFromRegionalEngines = 7,
    kByIdFallthrough = 8,
    kByMigrateToId = 9,

    kMaxValue = kByMigrateToId
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:ReconciliationType)

  // LINT.IfChange(ReconciliationVariant)
  enum class ReconciliationVariant {
    // Umbrella bucket
    kByID = 1,

    kByKeyword = 2,

    kByDomainBasedKeyword = 3,

    kMaxValue = kByDomainBasedKeyword
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/histograms.xml:ByReconciliationVariant)

  // Returns the keyword associated with currently set search engine.
  // If the SE definition comes from Play API, applies necessary changes
  // to compute keyword equivalent that can be matched with prepopulated
  // engines. Returns a pair of values:
  // - a string - keyword that can be matched with prepopulated_engines, and
  // - a ReconciliationType - indicating whether returned value had to be
  // computed from
  //   engine's Search URL and if so, which kind of change was made.
  std::pair<std::u16string, ReconciliationType> GetOrComputeKeyword() const;

  // Find Chrome built-in Search Engine definitions matching supplied |keyword|.
  // Use sparingly: this method may be moderately expensive to call:
  // - iterates lengthy, unsorted PrepopulatedEngine list,
  // - creates TemplateURLData equivalents from PrepopulatedEngine definitions,
  std::unique_ptr<TemplateURLData> FindMatchingBuiltInDefinitionsByKeyword(
      const std::u16string& keyword) const;

  // Find Chrome built-in Search Engine definitions matching supplied
  // |prepopulate_id|.
  // Use sparingly: this method may be moderately expensive to call:
  // - iterates lengthy, unsorted PrepopulatedEngine list,
  // - creates TemplateURLData equivalents from PrepopulatedEngine definitions,
  // Returns a pair of values:
  // - a TemplateURLData unique ptr - the definition that was found, or nullptr
  // otherwise
  // - a ReconciliationType - indicating the type of reconciliation that was
  // used to search for the definition.
  std::pair<std::unique_ptr<TemplateURLData>, ReconciliationType>
  FindMatchingBuiltInDefinitionsById(
      const TemplateURLData& data_to_match) const;

 private:
  raw_ref<TemplateURLPrepopulateData::Resolver> prepopulate_data_resolver_;
  std::unique_ptr<TemplateURLData> search_engine_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_RECONCILING_TEMPLATE_URL_DATA_HOLDER_H_
