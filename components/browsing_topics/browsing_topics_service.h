// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_SERVICE_H_
#define COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_SERVICE_H_

#include "components/browsing_topics/annotator.h"
#include "components/browsing_topics/mojom/browsing_topics_internals.mojom-forward.h"
#include "components/browsing_topics/mojom/browsing_topics_internals.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom-forward.h"
#include "url/origin.h"

namespace browsing_topics {

// A profile keyed service for providing the topics to a requesting context or
// to other internal components (e.g. UX).
class BrowsingTopicsService : public KeyedService {
 public:
  // Writes the browsing topics for a particular requesting context into the
  // output parameter `topics` and returns whether the access permission is
  // allowed. `context_origin` and `main_frame` will potentially be used for the
  // access permission check, for calculating the topics, and/or for the
  // `BrowsingTopicsPageLoadDataTracker` to track the API usage. If `get_topics`
  // is true, topics calculation result will be stored to `topics`. If `observe`
  // is true, record the observation (i.e. the <calling context site,
  // top level site> pair) to the `BrowsingTopicsSiteDataStorage` database.
  virtual bool HandleTopicsWebApi(
      const url::Origin& context_origin,
      content::RenderFrameHost* main_frame,
      ApiCallerSource caller_source,
      bool get_topics,
      bool observe,
      std::vector<blink::mojom::EpochTopicPtr>& topics) = 0;

  // Returns the number of distinct epochs versions for `main_frame_origin`.
  // Must be called when topics are eligible (i.e. `HandleTopicsWebApi` would
  // return true for the same main frame context).
  virtual int NumVersionsInEpochs(
      const url::Origin& main_frame_origin) const = 0;

  // Get the topics state to show in the chrome://topics-internals page. If
  // `calculate_now` is true, this will first trigger a calculation before
  // invoking `callback` with the topics state.
  virtual void GetBrowsingTopicsStateForWebUi(
      bool calculate_now,
      mojom::PageHandler::GetBrowsingTopicsStateCallback callback) = 0;

  // Return the top topics from all the past epochs. Up to
  // `kBrowsingTopicsNumberOfEpochsToExpose + 1` epochs' topics are kept in
  // the browser. Padded top topics won't be returned.
  virtual std::vector<privacy_sandbox::CanonicalTopic> GetTopTopicsForDisplay()
      const = 0;

  // Validates if the scheduled topics calculation ran on time, logging failure
  // metrics if not.
  //
  // Validation is skipped under the following conditions:
  //   - The topics data is not yet fully loaded.
  //   - A calculation is currently in progress.
  //   - The browser is in the process of shutting down.
  //   - Failure metrics for the current session have already been logged.
  virtual void ValidateCalculationSchedule() = 0;

  virtual Annotator* GetAnnotator() = 0;

  // Removes topic from any existing epoch.
  virtual void ClearTopic(
      const privacy_sandbox::CanonicalTopic& canonical_topic) = 0;

  // Clear the topics data (both raw and derived) for a specific context origin.
  virtual void ClearTopicsDataForOrigin(const url::Origin& origin) = 0;

  // Clear all topics data (both raw and derived).
  virtual void ClearAllTopicsData() = 0;

  ~BrowsingTopicsService() override = default;
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_SERVICE_H_
