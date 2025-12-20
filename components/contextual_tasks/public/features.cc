// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/features.h"

#include <string>
#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace contextual_tasks {

// Enables the contextual tasks side panel while browsing.
BASE_FEATURE(kContextualTasks, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables relevant context determination for contextual tasks.
BASE_FEATURE(kContextualTasksContext, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables integration with the server side context library.
BASE_FEATURE(kContextualTasksContextLibrary, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables quality logging for relevant context determination for contextual
// tasks.
BASE_FEATURE(kContextualTasksContextLogging, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables context menu settings for contextual tasks.
BASE_FEATURE(kContextualTasksContextMenu,
             "ContextualTasksContextMenu",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables suggestions for contextual tasks.
BASE_FEATURE(kContextualTasksSuggestionsEnabled,
             "ContextualTasksSuggestionsEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<double> kMinEmbeddingSimilarityScore{
    &kContextualTasksContext, "ContextualTasksContextEmbeddingSimilarityScore",
    0.8};

const base::FeatureParam<bool> kOnlyUseTitlesForSimilarity(
    &kContextualTasksContext,
    "ContextualTasksContextOnlyUseTitles",
    false);

const base::FeatureParam<double> kMinMultiSignalScore{
    &kContextualTasksContext, "ContextualTasksContextMinMultiSignalScore", 0.8};

const base::FeatureParam<double> kContentVisibilityThreshold{
    &kContextualTasksContext,
    "ContextualTasksContextContentVisibilityThreshold", 0.7};

const base::FeatureParam<double> kContextualTasksContextLoggingSampleRate{
    &kContextualTasksContextLogging, "ContextualTasksContextLoggingSampleRate",
    1.0};

// The base URL for the AI page.
const base::FeatureParam<std::string> kContextualTasksAiPageUrl{
    &kContextualTasksContext, "ai-page-url",
    "https://www.google.com/search?udm=50"};

// The host that any URL loaded in the embedded WebUi page will be routed to.
const base::FeatureParam<std::string> kContextualTasksForcedEmbeddedPageHost{
    &kContextualTasks, "forced-embedded-page-host", ""};

// The base domains for the sign in page.
const base::FeatureParam<std::string> kContextualTasksSignInDomains{
    &kContextualTasksContext, "sign-in-domains",
    "accounts.google.com,login.corp.google.com"};

constexpr base::FeatureParam<EntryPointOption>::Option kEntryPointOptions[] = {
    {EntryPointOption::kNoEntryPoint, "no-entry-point"},
    {EntryPointOption::kPageActionRevisit, "page-action-revisit"},
    {EntryPointOption::kToolbarRevisit, "toolbar-revisit"},
    {EntryPointOption::kToolbarPermanent, "toolbar-permanent"}};

const base::FeatureParam<EntryPointOption> kShowEntryPoint(
    &kContextualTasks,
    "ContextualTasksEntryPoint",
    EntryPointOption::kNoEntryPoint,
    &kEntryPointOptions);

const base::FeatureParam<bool> kTaskScopedSidePanel(&kContextualTasksContext,
                                                     "TaskScopedSidePanel",
                                                     true);

const base::FeatureParam<bool> kEnableLensInContextualTasks(
    &kContextualTasksContext,
    "EnableLensInContextualTasks",
    true);

const base::FeatureParam<bool> kForceGscInTabMode(&kContextualTasks,
                                                  "ForceGscInTabMode",
                                                  true);

// The user agent suffix to use for requests from the contextual tasks UI.
const base::FeatureParam<std::string> kContextualTasksUserAgentSuffix{
    &kContextualTasks, "user-agent-suffix", "Cobrowsing/1.0"};

const base::FeatureParam<bool> kEnableSteadyComposeboxVoiceSearch(
    &kContextualTasksContext,
    "EnableSteadyComposeboxVoiceSearch",
    true);

const base::FeatureParam<bool> kEnableExpandedComposeboxVoiceSearch(
    &kContextualTasksContext,
    "EnableExpandedComposeboxVoiceSearch",
    true);

bool GetIsExpandedComposeboxVoiceSearchEnabled() {
  return kEnableExpandedComposeboxVoiceSearch.Get();
}

bool GetIsSteadyComposeboxVoiceSearchEnabled() {
  return kEnableSteadyComposeboxVoiceSearch.Get();
}

bool ShouldForceGscInTabMode() {
  return kForceGscInTabMode.Get();
}

std::string GetContextualTasksAiPageUrl() {
  return kContextualTasksAiPageUrl.Get();
}

std::string GetForcedEmbeddedPageHost() {
  std::string host = kContextualTasksForcedEmbeddedPageHost.Get();

  // If there's a non-empty host, ensure that it is only ever going to a
  // google.com domain. If not, return the default empty string.
  if (!host.empty() && !base::EndsWith(host, ".google.com")) {
    return kContextualTasksForcedEmbeddedPageHost.default_value;
  }

  return host;
}

std::vector<std::string> GetContextualTasksSignInDomains() {
  return base::SplitString(kContextualTasksSignInDomains.Get(), ",",
                           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

bool GetIsContextualTasksNextboxContextMenuEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksContextMenu);
}

const base::FeatureParam<std::string> kContextualTasksNextboxImageFileTypes{
    &kContextualTasksContextMenu, "ContextualTasksNextboxImageFileTypes",
    "image/jpeg,image/png"};

const base::FeatureParam<std::string> kContextualTasksNextboxAttachmentFileTypes{
    &kContextualTasksContextMenu, "ContextualTasksNextboxAttachmentFileTypes",
    "text/plain,application/pdf"};

const base::FeatureParam<int> kContextualTasksNextboxMaxFileSize{
    &kContextualTasksContextMenu, "ContextualTasksNextboxMaxFileSize",
    20 * 1024 * 1024};

const base::FeatureParam<int> kContextualTasksNextboxMaxFileCount{
    &kContextualTasksContextMenu, "ContextualTasksNextboxMaxFileCount", 4};

bool GetIsContextualTasksSuggestionsEnabled() {
  return base::FeatureList::IsEnabled(kContextualTasksSuggestionsEnabled);
}

bool GetEnableLensInContextualTasks() {
  return base::FeatureList::IsEnabled(kContextualTasks) &&
         kEnableLensInContextualTasks.Get();
}

std::string GetContextualTasksUserAgentSuffix() {
  return kContextualTasksUserAgentSuffix.Get();
}

bool ShouldLogContextualTasksContextQuality() {
  if (!base::FeatureList::IsEnabled(kContextualTasksContextLogging)) {
    return false;
  }
  return base::RandDouble() <= kContextualTasksContextLoggingSampleRate.Get();
}

namespace flag_descriptions {

const char kContextualTasksName[] = "Contextual Tasks";
const char kContextualTasksDescription[] =
    "Enable the contextual tasks feature.";

const char kContextualTasksContextName[] = "Contextual Tasks Context";
const char kContextualTasksContextDescription[] =
    "Enables relevant context determination for contextual tasks.";

const char kContextualTasksContextLibraryName[] =
    "Contextual Tasks Context Library";
const char kContextualTasksContextLibraryDescription[] =
    "Enables integration with the server side context library.";

const char kContextualTasksSuggestionsEnabledName[] =
    "Contextual Tasks Suggestions Enabled";
const char kContextualTasksSuggestionsEnabledDescription[] =
    "Enables suggestions for contextual tasks.";

}  // namespace flag_descriptions

}  // namespace contextual_tasks
