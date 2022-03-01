// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/history_clusters_action.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/escape.h"

namespace history_clusters {

namespace {

class HistoryClustersAction : public OmniboxAction {
 public:
  explicit HistoryClustersAction(const std::string& query)
      : OmniboxAction(
            OmniboxAction::LabelStrings(
                GetConfig().alternate_omnibox_action_text
                    ? IDS_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH_HINT_ALTERNATE
                    : IDS_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH_HINT,
                GetConfig().alternate_omnibox_action_text
                    ? IDS_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH_SUGGESTION_CONTENTS_ALTERNATE
                    : IDS_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH_SUGGESTION_CONTENTS,
                GetConfig().alternate_omnibox_action_text
                    ? IDS_ACC_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH_SUFFIX_ALTERNATE
                    : IDS_ACC_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH_SUFFIX,
                GetConfig().alternate_omnibox_action_text
                    ? IDS_ACC_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH_ALTERNATE
                    : IDS_ACC_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH),
            GURL(base::StringPrintf(
                "chrome://history/journeys?q=%s",
                net::EscapeQueryParamValue(query, /*use_plus=*/false)
                    .c_str()))) {}

  void RecordActionShown(size_t position) const override {
    base::UmaHistogramExactLinear(
        "Omnibox.ResumeJourneyShown", position,
        AutocompleteResult::kMaxAutocompletePositionValue);
  }

  void RecordActionExecuted(size_t position) const override {
    base::UmaHistogramExactLinear(
        "Omnibox.SuggestionUsed.ResumeJourney", position,
        AutocompleteResult::kMaxAutocompletePositionValue);
  }

 private:
  ~HistoryClustersAction() override = default;
};

}  // namespace

void AttachHistoryClustersActions(
    history_clusters::HistoryClustersService* service,
    PrefService* prefs,
    AutocompleteResult& result) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Compile out this method for Mobile, which doesn't omnibox actions yet.
  // This is to prevent binary size increase for no reason.
  return;
#else
  if (!service)
    return;

  // Both features must be enabled to ever attach the action chip.
  if (!service->IsJourneysEnabled() || !GetConfig().omnibox_action) {
    return;
  }

  // History Clusters must be visible to the user to attach the action chip.
  if (!prefs->GetBoolean(history_clusters::prefs::kVisible)) {
    return;
  }

  for (auto& match : result) {
    // Skip incompatible matches (like entities) or ones with existing actions.
    // TODO(tommycli): Deduplicate this code with Pedals.
    if (match.action ||
        !AutocompleteMatch::IsActionCompatibleType(match.type)) {
      continue;
    }

    // Also skip all URLs. Only match this for Search matches. Pedals doesn't
    // explicitly filter URL matches out, but the "bag" of words it searches
    // over is quite small. That's why we need to be stricter than Pedals.
    if (!AutocompleteMatch::IsSearchType(match.type)) {
      continue;
    }

    std::string query = base::UTF16ToUTF8(match.contents);
    if (service->DoesQueryMatchAnyCluster(query)) {
      match.action = base::MakeRefCounted<HistoryClustersAction>(query);

      // Only ever attach one action (to the highest match), to not overwhelm
      // the user with multiple "Resume Journey" action buttons.
      return;
    }
  }
#endif
}

}  // namespace history_clusters
