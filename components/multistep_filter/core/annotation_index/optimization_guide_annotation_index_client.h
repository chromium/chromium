// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_OPTIMIZATION_GUIDE_ANNOTATION_INDEX_CLIENT_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_OPTIMIZATION_GUIDE_ANNOTATION_INDEX_CLIENT_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"

class GURL;

namespace optimization_guide {
class OptimizationGuideDecider;
class OptimizationMetadata;
}  // namespace optimization_guide

namespace multistep_filter {

class MultistepFilterLogRouter;
struct FilterAnnotation;
struct FilterSuggestionCandidate;

// `OptimizationGuideAnnotationIndexClient` serves as an implementation of
// `AnnotationIndexClient` that retrieves supported tasks, extracted filter
// annotations, and suggestion execution strategies via the
// `OptimizationGuideDecider`.
//
// This class abstracts hint registration and querying from the core
// `multistep_filter` logic. It achieves this by:
//  - Registering required optimization types with `OptimizationGuideDecider`.
//  - Querying the local optimization guide hints and metadata for given URLs.
//  - Translating optimization guide hint data into `multistep_filter` data
//    models.
class OptimizationGuideAnnotationIndexClient : public AnnotationIndexClient {
 public:
  static std::unique_ptr<OptimizationGuideAnnotationIndexClient> Create(
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      MultistepFilterLogRouter* log_router);

  OptimizationGuideAnnotationIndexClient(
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      MultistepFilterLogRouter* log_router);
  ~OptimizationGuideAnnotationIndexClient() override;

  // AnnotationIndexClient overrides:
  void GetFilterSuggestionCandidates(
      const GURL& url,
      base::span<const FilterAnnotation> filter_annotations,
      base::OnceCallback<
          void(std::optional<std::vector<FilterSuggestionCandidate>>)> callback,
      int64_t navigation_id) override;

  void GetSupportedTasks(
      const GURL& url,
      base::OnceCallback<void(std::vector<std::string>)> callback,
      int64_t navigation_id) override;

  void ExtractFilterAnnotation(
      const GURL& url,
      base::OnceCallback<void(std::optional<FilterAnnotation>)> callback,
      int64_t navigation_id) override;

 private:
  // The `OptimizationGuideDecider` used to query the optimization guide hints.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_;

  // Log router for the internals page.
  raw_ptr<MultistepFilterLogRouter> log_router_;

  // Callbacks invoked by `OptimizationGuideDecider` when queried for
  // optimization types. Parses `metadata`, logs the result to `log_router_`,
  // and invokes `callback` with the result.
  void OnFilterExecutionStrategyDecision(
      int64_t navigation_id,
      scoped_refptr<base::RefCountedData<base::OnceCallback<
          void(std::optional<std::vector<FilterSuggestionCandidate>>)>>>
          shared_callback,
      const GURL& url,
      const base::flat_map<
          optimization_guide::proto::OptimizationType,
          optimization_guide::OptimizationGuideDecisionWithMetadata>&
          decisions);

  void OnFilterTasksSupportedDecision(
      const GURL& url,
      int64_t navigation_id,
      base::OnceCallback<void(std::vector<std::string>)> callback,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  void OnFilterExtractAttributesDecision(
      const GURL& url,
      int64_t navigation_id,
      base::OnceCallback<void(std::optional<FilterAnnotation>)> callback,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Registers the optimization types with the `OptimizationGuideDecider`.
  void RegisterOptimizationTypes();

  // This should be kept at the end so that it is the first member to be
  // destroyed.
  base::WeakPtrFactory<OptimizationGuideAnnotationIndexClient>
      weak_ptr_factory_{this};
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_OPTIMIZATION_GUIDE_ANNOTATION_INDEX_CLIENT_H_
