// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_SERVICE_IMPL_H_
#define COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_SERVICE_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "components/browsing_topics/annotator.h"
#include "components/browsing_topics/browsing_topics_calculator.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/browsing_topics/browsing_topics_state.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"

namespace content {
class BrowsingTopicsSiteDataManager;
}  // namespace content

namespace browsing_topics {

// A profile keyed service for scheduling browsing topics calculation,
// calculating the topics to give to a requesting context or to other internal
// components (e.g. UX), and handling relevant data deletion. Browsing topics
// calculation will happen periodically every time period of
// `kBrowsingTopicsTimePeriodPerEpoch`. See the `BrowsingTopicsCalculator` class
// for the calculation details.
class BrowsingTopicsServiceImpl
    : public BrowsingTopicsService,
      public privacy_sandbox::PrivacySandboxSettings::Observer,
      public history::HistoryServiceObserver {
 public:
  BrowsingTopicsServiceImpl(const BrowsingTopicsServiceImpl&) = delete;
  BrowsingTopicsServiceImpl& operator=(const BrowsingTopicsServiceImpl&) =
      delete;
  BrowsingTopicsServiceImpl(BrowsingTopicsServiceImpl&&) = delete;
  BrowsingTopicsServiceImpl& operator=(BrowsingTopicsServiceImpl&&) = delete;

  ~BrowsingTopicsServiceImpl() override;

  bool HandleTopicsWebApi(
      const url::Origin& context_origin,
      content::RenderFrameHost* main_frame,
      ApiCallerSource caller_source,
      bool get_topics,
      bool observe,
      std::vector<blink::mojom::EpochTopicPtr>& topics) override;

  int NumVersionsInEpochs(const url::Origin& main_frame_origin) const override;

  void GetBrowsingTopicsStateForWebUi(
      bool calculate_now,
      mojom::PageHandler::GetBrowsingTopicsStateCallback callback) override;

  std::vector<privacy_sandbox::CanonicalTopic> GetTopTopicsForDisplay()
      const override;

  Annotator* GetAnnotator() override;

  void ClearTopic(
      const privacy_sandbox::CanonicalTopic& canonical_topic) override;

  void ClearTopicsDataForOrigin(const url::Origin& origin) override;

  void ClearAllTopicsData() override;

 protected:
  // The following methods are marked protected so that they may be overridden
  // by tests.

  virtual std::unique_ptr<BrowsingTopicsCalculator> CreateCalculator(
      privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
      history::HistoryService* history_service,
      content::BrowsingTopicsSiteDataManager* site_data_manager,
      Annotator* annotator,
      const base::circular_deque<EpochTopics>& epochs,
      BrowsingTopicsCalculator::CalculateCompletedCallback callback);

  // Allow tests to access `browsing_topics_state_`.
  virtual const BrowsingTopicsState& browsing_topics_state();

  // privacy_sandbox::PrivacySandboxSettings::Observer:
  //
  // When the floc-accessible-since time is updated (due to e.g. cookies
  // deletion), we'll invalidate the underlying browsing topics.
  void OnTopicsDataAccessibleSinceUpdated() override;

  // history::HistoryServiceObserver:
  //
  // On history deletion, the top topics of history epochs will be invalidated
  // if the deletion time range overlaps with the time range of the underlying
  // data used to derive the topics.
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  // Called when the outstanding calculation completes. It's going to reset
  // `topics_calculator_`, add the new `epoch_topics` to `browsing_topics_`, and
  // schedule the next calculation.
  virtual void OnCalculateBrowsingTopicsCompleted(EpochTopics epoch_topics);

 private:
  friend class BrowsingTopicsServiceFactory;
  friend class BrowsingTopicsBrowserTest;
  friend class TesterBrowsingTopicsService;

  using TopicAccessedCallback =
      base::RepeatingCallback<void(content::RenderFrameHost* rfh,
                                   const url::Origin& api_origin,
                                   bool blocked_by_policy,
                                   privacy_sandbox::CanonicalTopic topic)>;

  BrowsingTopicsServiceImpl(
      const base::FilePath& profile_path,
      privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
      history::HistoryService* history_service,
      content::BrowsingTopicsSiteDataManager* site_data_manager,
      std::unique_ptr<Annotator> annotator,
      TopicAccessedCallback topic_accessed_callback);

  void ScheduleBrowsingTopicsCalculation(base::TimeDelta delay);

  // Initialize `topics_calculator_` to start calculating this epoch's top
  // topics and context observed topics.
  void CalculateBrowsingTopics();

  // Set `browsing_topics_state_loaded_` to true. Start scheduling the topics
  // calculation.
  void OnBrowsingTopicsStateLoaded();

  // KeyedService:
  void Shutdown() override;

  mojom::WebUIGetBrowsingTopicsStateResultPtr
  GetBrowsingTopicsStateForWebUiHelper();

  // These pointers are safe to hold and use throughout the lifetime of
  // `this`:
  // - For `privacy_sandbox_settings_`, `history_service_`: the dependency
  // declared in `BrowsingTopicsServiceFactory`'s constructor guarantees that
  // `BrowsingTopicsService` will be destroyed first before those depend-on
  // services.
  // - For `site_data_manager_`: it lives in the StoragePartition which lives
  // in the BrowserContext, and thus outlives all BrowserContext's KeyedService.
  raw_ptr<privacy_sandbox::PrivacySandboxSettings> privacy_sandbox_settings_;
  raw_ptr<history::HistoryService> history_service_;
  raw_ptr<content::BrowsingTopicsSiteDataManager> site_data_manager_;

  BrowsingTopicsState browsing_topics_state_;

  // Whether the `browsing_topics_state_` has finished loading. Before the
  // loading finishes, accessor methods will use a default handling (i.e. return
  // an empty value; skip usage tracking; ignore data deletions). This is fine
  // in practice, as the loading should be reasonably fast, and normally the API
  // usage or data deletion won't happen at the browser start.
  bool browsing_topics_state_loaded_ = false;

  // Owns the ML model and all associated logic. Its lifetime is the same as
  // |this| so that the model can be downloaded as early as possible after the
  // start of a browsing session.
  std::unique_ptr<Annotator> annotator_;

  // This is non-null if a calculation is in progress. A calculation can be
  // triggered periodically, or due to the "Calculate Now" request from the
  // WebUI.
  std::unique_ptr<BrowsingTopicsCalculator> topics_calculator_;

  // This is populated when a request for the topics state arrives during an
  // ongoing topics calculation, or for a request that requires "Calculate Now"
  // in the first place. Callbacks will be invoked to return the latest topics
  // state as soon as the ongoing calculation finishes, and
  // `get_state_for_webui_callbacks_` will be cleared afterwards.
  std::vector<mojom::PageHandler::GetBrowsingTopicsStateCallback>
      get_state_for_webui_callbacks_;

  base::OneShotTimer schedule_calculate_timer_;

  TopicAccessedCallback topic_accessed_callback_;

  base::ScopedObservation<privacy_sandbox::PrivacySandboxSettings,
                          privacy_sandbox::PrivacySandboxSettings::Observer>
      privacy_sandbox_settings_observation_{this};

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  base::WeakPtrFactory<BrowsingTopicsServiceImpl> weak_ptr_factory_{this};
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_SERVICE_IMPL_H_
