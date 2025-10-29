// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/features.h"

#include <string>
#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"

namespace contextual_tasks {

// Enables the contextual tasks side panel while browsing.
BASE_FEATURE(kContextualTasks, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables relevant context determination for contextual tasks.
BASE_FEATURE(kContextualTasksContext, base::FEATURE_DISABLED_BY_DEFAULT);

// The base URL for the AI page.
const base::FeatureParam<std::string> kContextualTasksAiPageUrl{
    &kContextualTasksContext, "ai-page-url",
    "https://www.google.com/search?udm=50"};

// The base domains for the sign in page.
const base::FeatureParam<std::string> kContextualTasksSignInDomains{
    &kContextualTasksContext, "sign-in-domains",
    "accounts.google.com,login.corp.google.com"};

const base::FeatureParam<double> kMinEmbeddingSimilarityScore{
    &kContextualTasksContext, "ContextualTasksContextEmbeddingSimilarityScore",
    0.85};

const base::FeatureParam<bool> kOnlyUseTitlesForSimilarity(
    &kContextualTasksContext,
    "ContextualTasksContextOnlyUseTitles",
    false);

std::string GetContextualTasksAiPageUrl() {
  return kContextualTasksAiPageUrl.Get();
}

std::vector<std::string> GetContextualTasksSignInDomains() {
  return base::SplitString(kContextualTasksSignInDomains.Get(), ",",
                           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

namespace flag_descriptions {

const char kContextualTasksName[] = "Contextual Tasks";
const char kContextualTasksDescription[] =
    "Enable the contextual tasks feature.";

const char kContextualTasksContextName[] = "Contextual Tasks Context";
const char kContextualTasksContextDescription[] =
    "Enables relevant context determination for contextual tasks.";

}  // namespace flag_descriptions

}  // namespace contextual_tasks
