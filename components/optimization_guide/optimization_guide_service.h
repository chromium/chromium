// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/version.h"
#include "components/optimization_guide/optimization_guide_service_observer.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace optimization_guide {

// Processes the hints downloaded from the Component Updater as part of the
// Optimization Hints component.
class OptimizationGuideService {
 public:
  // Enumerates the possible outcomes of processing hints. Used in UMA
  // histograms, so the order of enumerators should not be changed.
  //
  // Keep in sync with OptimizationGuideProcessHintsResult in
  // tools/metrics/histograms/enums.xml.
  enum class ProcessHintsResult {
    SUCCESS,
    FAILED_INVALID_PARAMETERS,
    FAILED_READING_FILE,
    FAILED_INVALID_CONFIGURATION,

    // Insert new values before this line.
    MAX,
  };

  explicit OptimizationGuideService(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread_task_runner);

  virtual ~OptimizationGuideService();

  void AddObserver(OptimizationGuideServiceObserver* observer);
  // Virtual so it can be mocked out in tests.
  virtual void RemoveObserver(OptimizationGuideServiceObserver* observer);

  // Processes hints from the given unindexed hints, unless its |hints_version|
  // matches that of the most recently parsed version, in which case it does
  // nothing.
  //
  // Virtual so it can be mocked out in tests.
  virtual void ProcessHints(const ComponentInfo& component_info);

  // Sets the latest processed version for testing.
  void SetLatestProcessedVersionForTesting(const base::Version& version);

 private:
  // Always called as part of a BEST_EFFORT priority task.
  void ProcessHintsInBackground(const ComponentInfo& component_info);

  // Adds the observer on UI thread.
  void AddObserverOnUIThread(OptimizationGuideServiceObserver* observer);

  // Removes the observer on UI thread, if present.
  void RemoveObserverOnUIThread(OptimizationGuideServiceObserver* observer);

  // Dispatches hints to listeners on UI thread.
  void DispatchHintsOnUIThread(const proto::Configuration& config,
                               const ComponentInfo& component_info);

  // Runner for indexing tasks.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Runner for UI Thread tasks.
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner_;

  // Observers receiving notifications on hints being processed.
  base::ObserverList<OptimizationGuideServiceObserver>::Unchecked observers_;

  base::Version latest_processed_version_;

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideService);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_H_
