// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_SERVICE_H_
#define COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_SERVICE_H_

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
  // Return the browsing topics for a particular requesting context. The
  // calling context and top context information will also be used for the
  // access permission check, and for the `BrowsingTopicsPageLoadDataTracker` to
  // track the API usage. If `observe` is true, record the observation
  // (i.e. the <calling context site, top level site> pair) to the
  // `BrowsingTopicsSiteDataStorage` database.
  virtual std::vector<blink::mojom::EpochTopicPtr> GetBrowsingTopicsForJsApi(
      const url::Origin& context_origin,
      content::RenderFrameHost* main_frame,
      bool observe) = 0;

  // Get the topics state to show in the chrome://topics-internals page. If
  // `calculate_now` is true, this will first trigger a calculation before
  // invoking `callback` with the topics state.
  virtual void GetBrowsingTopicsStateForWebUi(
      bool calculate_now,
      mojom::PageHandler::GetBrowsingTopicsStateCallback callback) = 0;

  // Return the topics (i.e. one topic from each epoch) that can be potentially
  // exposed to a given site. Up to `kBrowsingTopicsNumberOfEpochsToExpose`
  // epochs' topics can be returned. Padded top topics or random topics won't be
  // returned.
  virtual std::vector<privacy_sandbox::CanonicalTopic>
  GetTopicsForSiteForDisplay(const url::Origin& top_origin) const = 0;

  // Return the top topics from all the past epochs. Up to
  // `kBrowsingTopicsNumberOfEpochsToExpose + 1` epochs' topics are kept in
  // the browser. Padded top topics won't be returned.
  virtual std::vector<privacy_sandbox::CanonicalTopic> GetTopTopicsForDisplay()
      const = 0;

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
