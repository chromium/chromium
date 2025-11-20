// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_FEATURES_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_FEATURES_H_

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace contextual_tasks {

BASE_DECLARE_FEATURE(kContextualTasks);
BASE_DECLARE_FEATURE(kContextualTasksContext);

// Enum denoting which entry point can show when enabled.
enum class EntryPointOption {
  kNoEntryPoint,
  kPageActionRevisit,
  kToolbarRevisit,
  kToolbarPermanent
};

// The minimum score required for two embeddings to be considered similar.
extern const base::FeatureParam<double> kMinEmbeddingSimilarityScore;
// Whether to only consider titles for similarity.
extern const base::FeatureParam<bool> kOnlyUseTitlesForSimilarity;
// Controls whether the contextual task page action should show
extern const base::FeatureParam<EntryPointOption, true> kShowEntryPoint;

// Minimum score, computed using multiple signals, to consider a tab relevant.
extern const base::FeatureParam<double> kMinMultiSignalScore;

// If true, the side panel is task scoped. Meaning that for all tabs associated
// with the same task, they will share the same side panel. If the side panel
// changed to another task for one tab, all tabs associated with the former task
// will become associated with the new task. When set to false, task change in
// the side panel only affects the current tab.
extern const base::FeatureParam<bool> kTaskScopedSidpePanel;

// Returns the base URL for the AI page.
extern std::string GetContextualTasksAiPageUrl();

// Returns the domains for the sign in page.
extern std::vector<std::string> GetContextualTasksSignInDomains();

// Returns whether Lens is enabled in contextual tasks. When this is enabled,
// Lens entry points will open results in the contextual tasks panels.
extern bool GetEnableLensInContextualTasks();

namespace flag_descriptions {

extern const char kContextualTasksName[];
extern const char kContextualTasksDescription[];

extern const char kContextualTasksContextName[];
extern const char kContextualTasksContextDescription[];

}  // namespace flag_descriptions

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_FEATURES_H_
