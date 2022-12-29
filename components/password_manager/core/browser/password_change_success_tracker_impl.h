// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SUCCESS_TRACKER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SUCCESS_TRACKER_IMPL_H_

#include "components/password_manager/core/browser/password_change_success_tracker.h"

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "url/gurl.h"

class PrefService;

namespace password_manager {

// Observer-like interface for metric recordering by
// |PasswordChangeSuccessTrackerImpl|. This allows easier testing and
// separately adding support for UMA and UKM recording.
class PasswordChangeMetricsRecorder {
 public:
  virtual ~PasswordChangeMetricsRecorder() = default;

  // Record a password change flow whose top level domain plus 1 is
  // |etld_plus_1|.
  virtual void OnFlowRecorded(
      const std::string& etld_plus_1,
      PasswordChangeSuccessTracker::StartEvent start_event,
      PasswordChangeSuccessTracker::EndEvent end_event,
      PasswordChangeSuccessTracker::EntryPoint entry_point,
      base::TimeDelta duration) = 0;
};

// Implementation of |PasswordChangeMetricsRecorder| for UMA metrics.
class PasswordChangeMetricsRecorderUma : public PasswordChangeMetricsRecorder {
 public:
  static constexpr char kUmaKey[] =
      "PasswordManager.PasswordChangeFlowDuration";

  PasswordChangeMetricsRecorderUma() = default;
  ~PasswordChangeMetricsRecorderUma() override;

  PasswordChangeMetricsRecorderUma(const PasswordChangeMetricsRecorderUma&) =
      delete;
  PasswordChangeMetricsRecorderUma& operator=(
      const PasswordChangeMetricsRecorderUma&) = delete;

  // PasswordChangeMetricsRecorder:
  void OnFlowRecorded(const std::string& etld_plus_1,
                      PasswordChangeSuccessTracker::StartEvent start_event,
                      PasswordChangeSuccessTracker::EndEvent end_event,
                      PasswordChangeSuccessTracker::EntryPoint entry_point,
                      base::TimeDelta duration) override;
};

// Implementation of the |PasswordChangeMetricsRecorder| for UKM metrics.
// It currently does not associate the record with the current navigation;
// instead, it writes everything to the id |ukm::NoUrlSourceId()|.
class PasswordChangeMetricsRecorderUkm : public PasswordChangeMetricsRecorder {
 public:
  // The exponential factor used for bucket spacing for the UKM recorder.
  // Choosing a factor of 1.1 gives 70 unique buckets between 1 and 3600.
  // A sufficient good resolution is important, since we expect the majority of
  // flows to have durations much shorter than 3600 seconds.
  static constexpr double kBucketSpacing = 1.1;

  PasswordChangeMetricsRecorderUkm() = default;
  ~PasswordChangeMetricsRecorderUkm() override;

  PasswordChangeMetricsRecorderUkm(const PasswordChangeMetricsRecorderUkm&) =
      delete;
  PasswordChangeMetricsRecorderUkm& operator=(
      const PasswordChangeMetricsRecorderUkm&) = delete;

  // PasswordChangeMetricsRecorder:
  void OnFlowRecorded(const std::string& etld_plus_1,
                      PasswordChangeSuccessTracker::StartEvent start_event,
                      PasswordChangeSuccessTracker::EndEvent end_event,
                      PasswordChangeSuccessTracker::EntryPoint entry_point,
                      base::TimeDelta duration) override;
};

// Implementation of the |PasswordChangeSuccessTracker| interface.
class PasswordChangeSuccessTrackerImpl
    : public password_manager::PasswordChangeSuccessTracker {
 public:
  // Current record version for flows that are persisted in preferences.
  static constexpr int kTrackerVersion = 1;

  // Describes a manually started flow for which no information on the exact
  // |StartEvent| has been received yet.
  struct IncompleteFlow {
    IncompleteFlow(const std::string& etld_plus_1,
                   const std::string& username,
                   EntryPoint entry_point);
    // The url is stored as a string, since that is what |base::Value| supports.
    std::string etld_plus_1;
    std::string username;
    EntryPoint entry_point;
    base::Time start_time;
  };

  // Provides helper functions for obtaining the flow properties such as |url|
  // or |username| from the underlying |Value::Dict| object. Requires
  // the raw pointer passed to the constructor to outlive the |FlowView|.
  class FlowView {
   public:
    explicit FlowView(const base::Value::Dict* value);

    std::string GetEtldPlus1() const;
    std::string GetUsername() const;
    StartEvent GetStartEvent() const;
    EntryPoint GetEntryPoint() const;
    base::Time GetStartTime() const;

   private:
    // Reference to the underlying |Value::Dict|, which must outlive the
    // |FlowView|.
    const raw_ptr<const base::Value::Dict> value_;
  };

  explicit PasswordChangeSuccessTrackerImpl(PrefService* pref_service);

  ~PasswordChangeSuccessTrackerImpl() override;

  // PasswordChangeSuccessTracker:
  void OnManualChangePasswordFlowStarted(const GURL& url,
                                         const std::string& username,
                                         EntryPoint entry_point) override;
  void OnChangePasswordFlowModified(const GURL& url,
                                    StartEvent new_event_type) override;
  void OnChangePasswordFlowCompleted(const GURL& url,
                                     const std::string& username,
                                     EndEvent event_type,
                                     bool phished) override;

  // Add a |PasswordChangeMetricsRecorder| to listen for |OnFlowRecorded()|
  // events. The caller passes ownership to the |PasswordChangeSuccessTracker|.
  void AddMetricsRecorder(
      std::unique_ptr<PasswordChangeMetricsRecorder> recorder);

  // Convert the |url| to eTLD+1 serialized as a string. Exposed as a static
  // method for easier testing.
  static std::string ExtractEtldPlus1(const GURL& url);

 private:
  // Remove incomplete flows that have been around for longer than
  // |kFlowTypeRefinementTimeout|.
  void RemoveIncompleteFlowsWithTimeout();

  // Remove and record flows that have not been completed within |kFlowTimeout|.
  void RemoveFlowsWithTimeout(base::Value::List& flows);

  // Record a completed or timed out flow.
  void RecordMetrics(const std::string& etld_plus_1,
                     StartEvent start_event,
                     EndEvent end_event,
                     EntryPoint entry_point,
                     base::TimeDelta duration);

  // Pointer to the |PrefService| used for persisting events.
  raw_ptr<PrefService> pref_service_;

  // Manually changed flows for which the |StartEvent| is not yet known,
  // which are waiting for a |OnChangePasswordFlowModified()| call.
  // These are not persisted across restarts and therefore only kept in
  // memory. The events are in increasing order by their creation time.
  base::circular_deque<IncompleteFlow> incomplete_manual_flows_;

  // A list of |PasswordChangeMetricsRecorders| that process
  // |OnFlowRecorded()| events. For simplicity, they are owned by the
  // |PasswordChangeSuccessTracker|.
  std::vector<std::unique_ptr<PasswordChangeMetricsRecorder>>
      metrics_recorders_;
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SUCCESS_TRACKER_IMPL_H_
