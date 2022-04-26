// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/history_clusters_action.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/omnibox/browser/actions/omnibox_pedal_jni_wrapper.h"
#include "url/android/gurl_android.h"
#endif

namespace history_clusters {

namespace {

class HistoryClustersAction : public OmniboxAction {
 public:
  explicit HistoryClustersAction(const std::string& query)
      : OmniboxAction(
            OmniboxAction::LabelStrings(
                IDS_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH_HINT,
                IDS_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH_SUFFIX,
                IDS_ACC_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH),
            GURL(base::StringPrintf(
                "chrome://history/journeys?q=%s",
                base::EscapeQueryParamValue(query, /*use_plus=*/false)
                    .c_str()))) {
#if BUILDFLAG(IS_ANDROID)
    CreateOrUpdateJavaObject(query);
#endif
  }

  void RecordActionShown(size_t position, bool executed) const override {
    base::UmaHistogramExactLinear(
        "Omnibox.ResumeJourneyShown", position,
        AutocompleteResult::kMaxAutocompletePositionValue);

    if (executed) {
      base::UmaHistogramExactLinear(
          "Omnibox.SuggestionUsed.ResumeJourney", position,
          AutocompleteResult::kMaxAutocompletePositionValue);
    }

    base::UmaHistogramBoolean("Omnibox.SuggestionUsed.ResumeJourneyCTR",
                              executed);
  }

  int32_t GetID() const override {
    return static_cast<int32_t>(OmniboxActionId::HISTORY_CLUSTERS);
  }

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> GetJavaObject() const override {
    return j_omnibox_action_;
  }

  void CreateOrUpdateJavaObject(const std::string& query) {
    j_omnibox_action_.Reset(BuildHistoryClustersAction(
        GetID(), strings_.hint, strings_.suggestion_contents,
        strings_.accessibility_suffix, strings_.accessibility_hint, url_,
        query));
  }
#endif

 private:
  ~HistoryClustersAction() override = default;
#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> j_omnibox_action_;
#endif
};

}  // namespace

void AttachHistoryClustersActions(
    history_clusters::HistoryClustersService* service,
    PrefService* prefs,
    AutocompleteResult& result) {
#if BUILDFLAG(IS_IOS)
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

    if (AutocompleteMatch::IsSearchType(match.type)) {
      std::string query = base::UTF16ToUTF8(match.contents);
      if (service->DoesQueryMatchAnyCluster(query)) {
        match.action = base::MakeRefCounted<HistoryClustersAction>(query);
      }
    } else if (GetConfig().omnibox_action_on_urls) {
      // We do the URL stripping here, because we need it to both execute the
      // query, as well as to feed it into the action chip so the chip navigates
      // to the right place (with the query pre-populated).
      std::string url_keyword =
          history_clusters::ComputeURLKeywordForLookup(match.destination_url);
      if (service->DoesURLMatchAnyCluster(url_keyword)) {
        match.action = base::MakeRefCounted<HistoryClustersAction>(url_keyword);
      }
    }

    // Only ever attach one action (to the highest match), to not overwhelm
    // the user with multiple "Resume Journey" action buttons.
    if (match.action) {
      return;
    }
  }
#endif
}

}  // namespace history_clusters
