// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_UI_SERVICE_H_
#define COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_UI_SERVICE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_data.h"
#include "components/blacklist/opt_out_blacklist/opt_out_store.h"
#include "components/previews/content/previews_decider_impl.h"
#include "components/previews/content/previews_optimization_guide.h"
#include "components/previews/core/previews_black_list.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_logger.h"
#include "net/nqe/effective_connection_type.h"
#include "services/network/public/cpp/network_quality_tracker.h"

class GURL;

namespace previews {
class PreviewsDeciderImpl;

// A class to manage the UI portion of inter-thread communication between
// previews/ objects. Created and used on the UI thread.
class PreviewsUIService
    : public network::NetworkQualityTracker::EffectiveConnectionTypeObserver {
 public:
  PreviewsUIService(
      std::unique_ptr<PreviewsDeciderImpl> previews_decider_impl,
      std::unique_ptr<blacklist::OptOutStore> previews_opt_out_store,
      std::unique_ptr<PreviewsOptimizationGuide> previews_opt_guide,
      const PreviewsIsEnabledCallback& is_enabled_callback,
      std::unique_ptr<PreviewsLogger> logger,
      blacklist::BlacklistData::AllowedTypesAndVersions allowed_previews,
      network::NetworkQualityTracker* network_quality_tracker);
  ~PreviewsUIService() override;

  // network::EffectiveConnectionTypeObserver:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override;

  // Adds a navigation to |url| to the black list with result |opt_out|.
  void AddPreviewNavigation(const GURL& url,
                            PreviewsType type,
                            bool opt_out,
                            uint64_t page_id);

  // Clears the history of the black list between |begin_time| and |end_time|.
  void ClearBlackList(base::Time begin_time, base::Time end_time);

  // Notifies |logger_| that |host| has been blacklisted at |time|. Virtualized
  // in testing.
  virtual void OnNewBlacklistedHost(const std::string& host, base::Time time);

  // Notifies |logger_| that the user blacklisted state has changed. Where
  // |blacklisted| is the new user blacklisted status. Virtualized in testing.
  virtual void OnUserBlacklistedStatusChange(bool blacklisted);

  // Notifies |logger_| that the blacklist is cleared at |time|. Virtualized in
  // testing.
  virtual void OnBlacklistCleared(base::Time time);

  // Change the status of whether to ignored or consider PreviewsBlackList
  // decisions in |previews_decider_impl_|. This method is called when users
  // interact with the UI (i.e. click on the "Ignore Blacklist" button).
  // Virtualized in testing.
  virtual void SetIgnorePreviewsBlacklistDecision(bool ignored);

  // Notifies |logger_| whether PreviewsBlackList decisions are ignored or not.
  // This method is listening for notification from PreviewsDeciderImpl for when
  // the blacklist ignore status is changed so that |logger_| can update all
  // PreviewsLoggerObservers so that multiple instances of the page have the
  // same status. Virtualized in testing.
  virtual void OnIgnoreBlacklistDecisionStatusChanged(bool ignored);

  // Log the navigation to PreviewsLogger. Virtualized in testing.
  virtual void LogPreviewNavigation(const GURL& url,
                                    PreviewsType type,
                                    bool opt_out,
                                    base::Time time,
                                    uint64_t page_id);

  // Log the determined previews eligibility decision |reason| to the
  // PreviewsLogger. |passed_reasons| is a collection of reason codes that
  // correspond to eligibility checks that were satisfied prior to determining
  // |reason| and so the opposite of these |passed_reasons| codes was true.
  // The method takes ownership of |passed_reasons|. |page_id| is generated
  // by PreviewsDeciderImpl, and used to group decisions into groups on the
  // page, messages that don't need to be grouped can pass in 0 as page_id.
  // Virtualized in testing.
  virtual void LogPreviewDecisionMade(
      PreviewsEligibilityReason reason,
      const GURL& url,
      base::Time time,
      PreviewsType type,
      std::vector<PreviewsEligibilityReason>&& passed_reasons,
      uint64_t page_id);

  // Returns the vector of subresource patterns whose loading should be blocked
  // when loading |document_gurl|. Each pattern is a substring to match
  // against the URL.
  std::vector<std::string> GetResourceLoadingHintsResourcePatternsToBlock(
      const GURL& document_gurl) const;

  // Expose the pointer to PreviewsLogger to extract logging messages. This
  // pointer's life time is the same as of |this|, and it is guaranteed to not
  // return null.
  PreviewsLogger* previews_logger() const;

  // Gets the decision making object for Previews triggering. Guaranteed to be
  // non-null.
  PreviewsDeciderImpl* previews_decider_impl() const;

  // When triggering previews, prevent long term black list rules.
  void SetIgnoreLongTermBlackListForServerPreviews(
      bool ignore_long_term_black_list_rules_allowed);

 private:
  // The decision making object for Previews triggering. Guaranteed to be
  // non-null.
  std::unique_ptr<previews::PreviewsDeciderImpl> previews_decider_impl_;

  // A log object to keep track of events such as previews navigations,
  // blacklist actions, etc.
  std::unique_ptr<PreviewsLogger> logger_;

  // Used to remove |this| from observing.
  network::NetworkQualityTracker* network_quality_tracker_;

  // The current EffectiveConnectionType estimate.
  net::EffectiveConnectionType current_effective_connection_type_ =
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PreviewsUIService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PreviewsUIService);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_UI_SERVICE_H_
