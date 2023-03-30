// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_change_success_tracker_impl.h"

#include "base/containers/circular_deque.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::RecordAction;
using base::StringPiece;
using base::UserMetricsAction;

namespace password_manager {

namespace {

constexpr char kKeyEtldPlus1[] = "etld_plus_1";
constexpr char kKeyUsername[] = "username";
constexpr char kKeyStartEvent[] = "start_event";
constexpr char kKeyEntryPoint[] = "entry_point";
constexpr char kKeyStartTime[] = "start_time";

// Overloaded helper methods to convert the |PasswordChangeSuccessTracker|
// enums to strings for building metrics keys.
StringPiece SerializeEnumForUma(
    PasswordChangeSuccessTracker::StartEvent event) {
  switch (event) {
    case PasswordChangeSuccessTracker::StartEvent::kDeprecatedAutomatedFlow:
      NOTREACHED();
      return ".AutomatedFlow";
    // Combine all manual flows for UMA reporting to reduce number of
    // histograms.
    case PasswordChangeSuccessTracker::StartEvent::kManualUnknownFlow:
      return ".ManualUnknownFlow";
    case PasswordChangeSuccessTracker::StartEvent::kManualWellKnownUrlFlow:
      return ".ManualWellKnownUrlFlow";
    case PasswordChangeSuccessTracker::StartEvent::kManualChangePasswordUrlFlow:
      return ".ManualChangePasswordUrlFlow";
    case PasswordChangeSuccessTracker::StartEvent::kManualHomepageFlow:
      return ".ManualHomepageFlow";
    case PasswordChangeSuccessTracker::StartEvent::
        kDeprecatedManualResetLinkFlow:
      NOTREACHED();
      return ".ManualResetLinkFlow";
  }
}

StringPiece SerializeEnumForUma(PasswordChangeSuccessTracker::EndEvent event) {
  switch (event) {
    // (DEPRECATED) Combine automated flow end events for UMA reporting.
    case PasswordChangeSuccessTracker::EndEvent::
        kDeprecatedAutomatedFlowGeneratedPasswordChosen:
    case PasswordChangeSuccessTracker::EndEvent::
        kDeprecatedAutomatedFlowOwnPasswordChosen:
      NOTREACHED();
      return ".AutomatedFlowPasswordChosen";
    case PasswordChangeSuccessTracker::EndEvent::
        kDeprecatedAutomatedFlowResetLinkRequested:
      NOTREACHED();
      return ".AutomatedFlowResetLinkRequested";
    // Combine manual flow end events for UMA reporting.
    case PasswordChangeSuccessTracker::EndEvent::
        kManualFlowGeneratedPasswordChosen:
    case PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen:
      return ".ManualFlowPasswordChosen";
    case PasswordChangeSuccessTracker::EndEvent::kTimeout:
      return ".Timeout";
  }
}

StringPiece SerializeEnumForUma(
    PasswordChangeSuccessTracker::EntryPoint entry_point) {
  switch (entry_point) {
    case PasswordChangeSuccessTracker::EntryPoint::kDeprecatedLeakWarningDialog:
      NOTREACHED();
      return ".LeakWarningDialog";
    case PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings:
      return ".LeakCheckInSettings";
  }
}

// Helper method to create a flow serialized as a |Value::Dict|.
base::Value::Dict CreateFlow(
    const std::string& etld_plus_1,
    const std::string& username,
    PasswordChangeSuccessTracker::StartEvent start_event,
    PasswordChangeSuccessTracker::EntryPoint entry_point,
    base::Time start_time) {
  base::Value::Dict flow;
  flow.Set(kKeyEtldPlus1, base::Value(etld_plus_1));
  flow.Set(kKeyUsername, base::Value(username));
  // Cast enums to ints, since they are one of the supported types of
  // |Value|.
  flow.Set(kKeyStartEvent, base::Value(static_cast<int>(start_event)));
  flow.Set(kKeyEntryPoint, base::Value(static_cast<int>(entry_point)));
  flow.Set(kKeyStartTime, base::TimeToValue(start_time));

  return flow;
}

//  Record a UserAction based on how the user performed the password update.
void RecordUserActionOnPhishedCredentialforUma(
    PasswordChangeSuccessTracker::EndEvent event) {
  switch (event) {
    // (DEPRECATED) Combine automated flow end events for UMA reporting.
    case PasswordChangeSuccessTracker::EndEvent::
        kDeprecatedAutomatedFlowGeneratedPasswordChosen:
    case PasswordChangeSuccessTracker::EndEvent::
        kDeprecatedAutomatedFlowOwnPasswordChosen:
    case PasswordChangeSuccessTracker::EndEvent::
        kDeprecatedAutomatedFlowResetLinkRequested:
      NOTREACHED();
      break;
    // Combine manual flow end events for UMA reporting.
    case PasswordChangeSuccessTracker::EndEvent::
        kManualFlowGeneratedPasswordChosen:
    case PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen:
      RecordAction(UserMetricsAction(
          "PasswordProtection.PasswordUpdated.ManualFlowPasswordChosen"));
      break;
    case PasswordChangeSuccessTracker::EndEvent::kTimeout:
      RecordAction(
          UserMetricsAction("PasswordProtection.PasswordUpdated.Timeout"));
  }
}
}  // namespace

PasswordChangeMetricsRecorderUma::~PasswordChangeMetricsRecorderUma() = default;

void PasswordChangeMetricsRecorderUma::OnFlowRecorded(
    const std::string& etld_plus_1,
    PasswordChangeSuccessTracker::StartEvent start_event,
    PasswordChangeSuccessTracker::EndEvent end_event,
    PasswordChangeSuccessTracker::EntryPoint entry_point,
    base::TimeDelta duration) {
  // Record metrics aggregated over end events.
  std::string entry_key =
      base::StrCat({kUmaKey, SerializeEnumForUma(entry_point),
                    SerializeEnumForUma(start_event)});
  UmaHistogramLongTimes100(entry_key, duration);

  // Record metrics specified by start and end events. This does not
  // differentiate between different manual starts and between own or generated
  // passwords.
  base::StrAppend(&entry_key, {SerializeEnumForUma(end_event)});
  UmaHistogramLongTimes100(entry_key, duration);
}

PasswordChangeMetricsRecorderUkm::~PasswordChangeMetricsRecorderUkm() = default;

void PasswordChangeMetricsRecorderUkm::OnFlowRecorded(
    const std::string& etld_plus_1,
    PasswordChangeSuccessTracker::StartEvent start_event,
    PasswordChangeSuccessTracker::EndEvent end_event,
    PasswordChangeSuccessTracker::EntryPoint entry_point,
    base::TimeDelta duration) {
  int64_t bucketed_duration =
      ukm::GetExponentialBucketMin(duration.InSeconds(), kBucketSpacing);
  ukm::builders::PasswordManager_PasswordChangeFlowDuration(
      ukm::NoURLSourceId())
      .SetStartEvent(static_cast<int64_t>(start_event))
      .SetEndEvent(static_cast<int64_t>(end_event))
      .SetEntryPoint(static_cast<int64_t>(entry_point))
      .SetDuration(bucketed_duration)
      .Record(ukm::UkmRecorder::Get());
}

PasswordChangeSuccessTrackerImpl::IncompleteFlow::IncompleteFlow(
    const std::string& etld_plus_1,
    const std::string& username,
    EntryPoint entry_point)
    : etld_plus_1(etld_plus_1),
      username(username),
      entry_point(entry_point),
      start_time(base::Time::Now()) {}

PasswordChangeSuccessTrackerImpl::FlowView::FlowView(
    const base::Value::Dict* value)
    : value_(value) {
  DCHECK(value_);
}

std::string PasswordChangeSuccessTrackerImpl::FlowView::GetEtldPlus1() const {
  const std::string* etld_plus_1 = value_->FindString(kKeyEtldPlus1);
  return etld_plus_1 ? *etld_plus_1 : std::string();
}

std::string PasswordChangeSuccessTrackerImpl::FlowView::GetUsername() const {
  const std::string* username = value_->FindString(kKeyUsername);
  return username ? *username : std::string();
}

PasswordChangeSuccessTracker::StartEvent
PasswordChangeSuccessTrackerImpl::FlowView::GetStartEvent() const {
  absl::optional<int> start_event = value_->FindInt(kKeyStartEvent);
  // The value should never be empty and be within the range of the enum.
  DCHECK(start_event.has_value());
  DCHECK(start_event.value() >= 0);
  DCHECK(start_event.value() <=
         static_cast<int>(PasswordChangeSuccessTracker::StartEvent::kMaxValue));

  return static_cast<PasswordChangeSuccessTracker::StartEvent>(
      start_event.value());
}

PasswordChangeSuccessTracker::EntryPoint
PasswordChangeSuccessTrackerImpl::FlowView::GetEntryPoint() const {
  absl::optional<int> entry_point = value_->FindInt(kKeyEntryPoint);
  // The value should never be empty and be within the range of the enum.
  DCHECK(entry_point.has_value());
  DCHECK(entry_point.value() >= 0);
  DCHECK(entry_point.value() <=
         static_cast<int>(PasswordChangeSuccessTracker::EntryPoint::kMaxValue));

  return static_cast<PasswordChangeSuccessTracker::EntryPoint>(
      entry_point.value_or(0));
}

base::Time PasswordChangeSuccessTrackerImpl::FlowView::GetStartTime() const {
  absl::optional<base::Time> start_time =
      base::ValueToTime(value_->Find(kKeyStartTime));
  DCHECK(start_time.has_value());
  return start_time.value_or(base::Time::Min());
}

PasswordChangeSuccessTrackerImpl::PasswordChangeSuccessTrackerImpl(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);
  // Check whether the saved entries belong to an old version. If so,
  // remove all old flows.
  if (pref_service->GetInteger(prefs::kPasswordChangeSuccessTrackerVersion) <
      kTrackerVersion) {
    pref_service->SetInteger(prefs::kPasswordChangeSuccessTrackerVersion,
                             kTrackerVersion);
    pref_service->ClearPref(prefs::kPasswordChangeSuccessTrackerFlows);
  }
}

PasswordChangeSuccessTrackerImpl::~PasswordChangeSuccessTrackerImpl() = default;

void PasswordChangeSuccessTrackerImpl::OnManualChangePasswordFlowStarted(
    const GURL& url,
    const std::string& username,
    EntryPoint entry_point) {
  RemoveIncompleteFlowsWithTimeout();
  incomplete_manual_flows_.emplace_back(ExtractEtldPlus1(url), username,
                                        entry_point);
}

void PasswordChangeSuccessTrackerImpl::OnChangePasswordFlowModified(
    const GURL& url,
    StartEvent new_event_type) {
  RemoveIncompleteFlowsWithTimeout();
  if (incomplete_manual_flows_.empty())
    return;

  // We always take the first match. We do not expect conflicts and, if they,
  // occur, the information for both flows should be nearly identical.
  if (auto it =
          base::ranges::find(incomplete_manual_flows_, ExtractEtldPlus1(url),
                             &IncompleteFlow::etld_plus_1);
      it != incomplete_manual_flows_.cend()) {
    ScopedListPrefUpdate update(pref_service_,
                                prefs::kPasswordChangeSuccessTrackerFlows);
    base::Value::List& flows = update.Get();
    RemoveFlowsWithTimeout(flows);

    flows.Append(
        base::Value(CreateFlow(it->etld_plus_1, it->username, new_event_type,
                               it->entry_point, it->start_time)));
    incomplete_manual_flows_.erase(it);
  }
}

void PasswordChangeSuccessTrackerImpl::OnChangePasswordFlowCompleted(
    const GURL& url,
    const std::string& username,
    EndEvent event_type,
    bool phished) {
  // If there are no ongoing change flows, return immediately to avoid disk
  // writes.
  const base::Value::List& read_flows =
      pref_service_->GetList(prefs::kPasswordChangeSuccessTrackerFlows);
  if (read_flows.empty())
    return;

  ScopedListPrefUpdate update(pref_service_,
                              prefs::kPasswordChangeSuccessTrackerFlows);
  base::Value::List& flows = update.Get();
  RemoveFlowsWithTimeout(flows);

  // In the unlikely case that there are two flows with the same eTLD+1 and
  // username, we take the last entry. The underlying assumption is that
  // the first flow was abandoned but has not timed out yet.
  std::string target_etld_plus_1 = ExtractEtldPlus1(url);
  for (size_t i = flows.size(); i-- > 0;) {
    FlowView view(&flows[i].GetDict());
    if (view.GetEtldPlus1() == target_etld_plus_1 &&
        view.GetUsername() == username) {
      RecordMetrics(view.GetEtldPlus1(), view.GetStartEvent(), event_type,
                    view.GetEntryPoint(),
                    base::Time::Now() - view.GetStartTime());
      flows.erase(flows.begin() + i);
      if (phished) {
        RecordUserActionOnPhishedCredentialforUma(event_type);
      }
      return;
    }
  }
}

void PasswordChangeSuccessTrackerImpl::AddMetricsRecorder(
    std::unique_ptr<PasswordChangeMetricsRecorder> recorder) {
  metrics_recorders_.push_back(std::move(recorder));
}

std::string PasswordChangeSuccessTrackerImpl::ExtractEtldPlus1(
    const GURL& url) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::PrivateRegistryFilter::
               INCLUDE_PRIVATE_REGISTRIES);
}

void PasswordChangeSuccessTrackerImpl::RemoveIncompleteFlowsWithTimeout() {
  base::Time now = base::Time::Now();
  while (!incomplete_manual_flows_.empty() &&
         now - incomplete_manual_flows_.front().start_time >=
             kFlowTypeRefinementTimeout) {
    incomplete_manual_flows_.pop_front();
  }
  // Also remove entries that are in the future (e.g., from a time change).
  while (!incomplete_manual_flows_.empty() &&
         incomplete_manual_flows_.back().start_time > now) {
    incomplete_manual_flows_.pop_back();
  }
}

// Assumes that |flows| is a reference to the list containing all currently
// active flows and that the calling method takes care of persisting these
// |flows|.
void PasswordChangeSuccessTrackerImpl::RemoveFlowsWithTimeout(
    base::Value::List& flows) {
  base::Time now = base::Time::Now();
  for (auto it = flows.begin(); it != flows.end();) {
    FlowView view(&it->GetDict());
    if (base::TimeDelta duration = now - view.GetStartTime();
        duration > kFlowTimeout) {
      RecordMetrics(view.GetEtldPlus1(), view.GetStartEvent(),
                    EndEvent::kTimeout, view.GetEntryPoint(), kFlowTimeout);
      it = flows.erase(it);
    } else {
      // Flows are expected to be ordered by their time of creation.
      break;
    }
  }
}

void PasswordChangeSuccessTrackerImpl::RecordMetrics(
    const std::string& etld_plus_1,
    StartEvent start_event,
    EndEvent end_event,
    EntryPoint entry_point,
    base::TimeDelta duration) {
  for (const auto& recorder : metrics_recorders_) {
    recorder->OnFlowRecorded(etld_plus_1, start_event, end_event, entry_point,
                             duration);
  }
}

}  // namespace password_manager
