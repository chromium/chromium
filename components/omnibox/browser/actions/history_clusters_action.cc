// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/history_clusters_action.h"

#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/memories_features.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/escape.h"

void AttachHistoryClustersActions(
    history_clusters::HistoryClustersService* service,
    AutocompleteResult& result) {
#if defined(OS_ANDROID) || defined(OS_IOS)
  // Compile out this method for Mobile, which doesn't omnibox actions yet.
  // This is to prevent binary size increase for no reason.
  return;
#else
  if (!service)
    return;

  if (!base::FeatureList::IsEnabled(history_clusters::kMemories))
    return;

  for (auto& match : result) {
    // Skip incomptatible matches (like entities) or ones with existing actions.
    // TODO(tommycli): Deduplicate this code with Pedals.
    if (match.action ||
        !AutocompleteMatch::IsActionCompatibleType(match.type)) {
      continue;
    }

    std::string query = base::UTF16ToUTF8(match.contents);
    if (service->DoesQueryMatchAnyCluster(query)) {
      match.action = base::MakeRefCounted<OmniboxAction>(
          OmniboxAction::LabelStrings(
              IDS_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH_HINT,
              IDS_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH_SUGGESTION_CONTENTS,
              IDS_ACC_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH_SUFFIX,
              IDS_ACC_OMNIBOX_ACTION_HISTORY_CLUSTERS_SEARCH),
          GURL(base::StringPrintf(
              "chrome://memories/?q=%s",
              net::EscapeQueryParamValue(query, /*use_plus=*/false).c_str())));

      // Only ever attach one action (to the highest match), to not overwhelm
      // the user with multiple "Resume Journey" action buttons.
      return;
    }
  }
#endif
}
