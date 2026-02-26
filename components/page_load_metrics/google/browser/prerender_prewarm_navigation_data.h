// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_PRERENDER_PREWARM_NAVIGATION_DATA_H_
#define COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_PRERENDER_PREWARM_NAVIGATION_DATA_H_

#include "base/supports_user_data.h"

namespace page_load_metrics {

// Holds information about DSE prewarm navigations. This is owned by
// NavigationHandle for navigations, and is owned by RenderProcessHost for DSE
// prewarm status.
class PrerenderPrewarmNavigationData : public base::SupportsUserData::Data {
 public:
  // The status of the DSE prewarm navigation. These values are persisted to
  // logs. Entries should not be renumbered and numeric values should never be
  // reused.
  // LINT.IfChange(PrerenderPrewarmNavigationStatus)
  enum class PrerenderPrewarmNavigationStatus {
    kNotPrewarmedNotReused = 0,
    kNotPrewarmedReused = 1,
    kPrewarmedNotReused = 2,
    kPrewarmedReused = 3,
    kMaxValue = kPrewarmedReused,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/page/enums.xml:PrerenderPrewarmNavigationStatus)

  // LINT.IfChange(PriorPrewarmCommitStatus)
  enum class PriorPrewarmCommitStatus {
    kNoPrerenderPrewarmNavigation = 0,
    kHasPrerenderPrewarmNavigationWithoutCommit = 1,
    kHasPrerenderPrewarmNavigationWithCommit = 2,
    kMaxValue = kHasPrerenderPrewarmNavigationWithCommit,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/page/enums.xml:PriorPrewarmCommitStatus)

  static PrerenderPrewarmNavigationData* Get(
      base::SupportsUserData* support_user_data);

  template <typename... Args>
  static PrerenderPrewarmNavigationData* GetOrCreate(
      base::SupportsUserData* support_user_data,
      Args&&... args) {
    if (!PrerenderPrewarmNavigationData::Get(support_user_data)) {
      support_user_data->SetUserData(
          PrerenderPrewarmNavigationData::kRenderProcessHostUserDataKey,
          std::make_unique<PrerenderPrewarmNavigationData>(
              std::forward<Args>(args)...));
    }
    return PrerenderPrewarmNavigationData::Get(support_user_data);
  }

  static PriorPrewarmCommitStatus GetPriorPrewarmCommitStatus(
      base::SupportsUserData* support_user_data);

  PrerenderPrewarmNavigationData() = default;
  explicit PrerenderPrewarmNavigationData(bool prewarm_committed);
  ~PrerenderPrewarmNavigationData() override;

  PrerenderPrewarmNavigationData(const PrerenderPrewarmNavigationData&) =
      delete;
  PrerenderPrewarmNavigationData& operator=(
      const PrerenderPrewarmNavigationData&) = delete;

  PrerenderPrewarmNavigationStatus GetNavigationStatus(
      bool render_process_host_reused) const;

  bool prewarm_committed() const { return prewarm_committed_; }

 private:
  // The key to store and retrieve this data from RenderProcessHost.
  static const void* const kRenderProcessHostUserDataKey;

  // True when webcontent had DSE prewarm.
  const bool prewarm_committed_ = false;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_PRERENDER_PREWARM_NAVIGATION_DATA_H_
