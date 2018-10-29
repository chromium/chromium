// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_OPTIMIZATION_GUIDE_H_
#define COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_OPTIMIZATION_GUIDE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/optimization_guide_service_observer.h"
#include "components/previews/content/previews_optimization_guide.h"
#include "components/previews/core/previews_experiments.h"
#include "url/gurl.h"

namespace optimization_guide {
namespace proto {
class Configuration;
}  // namespace proto
}  // namespace optimization_guide

namespace previews {

class PreviewsHints;
class PreviewsUserData;

using ResourceLoadingHintsCallback = base::OnceCallback<void(
    const GURL& document_gurl,
    const std::vector<std::string>& resource_patterns_to_block)>;

// A Previews optimization guide that makes decisions guided by hints received
// from the OptimizationGuideService.
class PreviewsOptimizationGuide
    : public optimization_guide::OptimizationGuideServiceObserver {
 public:
  // The embedder guarantees |optimization_guide_service| outlives |this|.
  PreviewsOptimizationGuide(
      optimization_guide::OptimizationGuideService* optimization_guide_service,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);

  ~PreviewsOptimizationGuide() override;

  // Returns whether |type| is whitelisted for |url|.|previews_data| can be
  // modified.
  // Virtual so it can be mocked in tests.
  virtual bool IsWhitelisted(PreviewsUserData* previews_data,
                             const GURL& url,
                             PreviewsType type) const;

  // Returns whether |type| is blacklisted for |url|.
  // Virtual so it can be mocked in tests.
  virtual bool IsBlacklisted(const GURL& url, PreviewsType type) const;

  // Returns whether |request| may have associated optimization hints
  // (specifically, PageHints). If so, but the hints are not available
  // synchronously, this method will request that they be loaded (from disk or
  // network).
  bool MaybeLoadOptimizationHints(const GURL& url,
                                  ResourceLoadingHintsCallback callback);

  // Logs UMA for whether the OptimizationGuide HintCache has a matching Hint
  // guidance for |url|. This is useful for measuring the effectiveness of the
  // page hints provided by Cacao.
  void LogHintCacheMatch(const GURL& url,
                         bool is_committed,
                         net::EffectiveConnectionType ect) const;

  // optimization_guide::OptimizationGuideServiceObserver implementation:
  void OnHintsProcessed(
      const optimization_guide::proto::Configuration& config,
      const optimization_guide::ComponentInfo& component_info) override;

 private:
  // Updates the hints to the latest hints sent by the Component Updater.
  void UpdateHints(std::unique_ptr<PreviewsHints> hints);

  // Handles a loaded hint. It checks if the |loaded_hint| has any page hint
  // that apply to |doucment_url|. If so, it looks for any applicable resource
  // loading hints and will call |callback| with the applicable resource loading
  // details if found.
  void OnLoadedHint(ResourceLoadingHintsCallback callback,
                    const GURL& document_url,
                    const optimization_guide::proto::Hint& loaded_hint) const;

  // The OptimizationGuideService that this guide is listening to. Not owned.
  optimization_guide::OptimizationGuideService* optimization_guide_service_;

  // Runner for IO thread tasks.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Background thread where hints processing should be performed.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // The current hints used for this optimization guide.
  std::unique_ptr<PreviewsHints> hints_;

  // Used to get |weak_ptr_| to self on the IO thread.
  base::WeakPtrFactory<PreviewsOptimizationGuide> io_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsOptimizationGuide);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_OPTIMIZATION_GUIDE_H_
