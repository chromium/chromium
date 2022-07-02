// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_HISTORY_CLUSTERS_ACTION_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_HISTORY_CLUSTERS_ACTION_H_

#include "build/build_config.h"
#include "components/history/core/browser/history_types.h"
#include "components/omnibox/browser/actions/omnibox_action.h"

class AutocompleteResult;
class PrefService;

namespace gfx {
struct VectorIcon;
}

namespace history_clusters {

class HistoryClustersService;

// Made public for testing.
class HistoryClustersAction : public OmniboxAction {
 public:
  HistoryClustersAction(
      const std::string& query,
      const history::ClusterKeywordData& matched_keyword_data);

  void RecordActionShown(size_t position, bool executed) const override;
  int32_t GetID() const override;
#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
  const gfx::VectorIcon& GetVectorIcon() const override;
#endif
#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> GetJavaObject() const override;

  void CreateOrUpdateJavaObject(const std::string& query);
#endif

 private:
  ~HistoryClustersAction() override;

  // Additional data of the matching keyword from the history clustering
  // service.
  history::ClusterKeywordData matched_keyword_data_;

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> j_omnibox_action_;
#endif
};

// If the feature is enabled, attaches any necessary History Clusters actions
// onto any relevant matches in `result`.
void AttachHistoryClustersActions(
    history_clusters::HistoryClustersService* service,
    PrefService* prefs,
    AutocompleteResult& result);

}  // namespace history_clusters

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_HISTORY_CLUSTERS_ACTION_H_
