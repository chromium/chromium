// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_change_success_tracker_impl.h"

#include "base/containers/circular_deque.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

namespace {

constexpr char kKeyEtld1[] = "etld1";
constexpr char kKeyUsername[] = "username";
constexpr char kKeyStartEvent[] = "start_event";
constexpr char kKeyEntryPoint[] = "entry_point";
constexpr char kKeyStartTime[] = "start_time";

// Helper method to create a flow serialized as a |Value::Dict|.
base::Value::Dict CreateFlow(
    const std::string& etld1,
    const std::string& username,
    PasswordChangeSuccessTracker::StartEvent start_event,
    PasswordChangeSuccessTracker::EntryPoint entry_point,
    base::Time start_time) {
  base::Value::Dict flow;
  flow.Set(kKeyEtld1, base::Value(etld1));
  flow.Set(kKeyUsername, base::Value(username));
  // Cast enums to ints, since they are one of the supported types of
  // |Value|.
  flow.Set(kKeyStartEvent, base::Value(static_cast<int>(start_event)));
  flow.Set(kKeyEntryPoint, base::Value(static_cast<int>(entry_point)));
  flow.Set(kKeyStartTime, base::TimeToValue(start_time));

  return flow;
}

}  // namespace

PasswordChangeSuccessTrackerImpl::IncompleteFlow::IncompleteFlow(
    const std::string& etld1,
    const std::string& username,
    EntryPoint entry_point)
    : etld1(etld1),
      username(username),
      entry_point(entry_point),
      start_time(base::Time::Now()) {}

PasswordChangeSuccessTrackerImpl::FlowView::FlowView(
    const base::Value::Dict* value)
    : value_(value) {
  DCHECK(value_);
}

std::string PasswordChangeSuccessTrackerImpl::FlowView::GetEtld1() const {
  const std::string* etld1 = value_->FindString(kKeyEtld1);
  return etld1 ? *etld1 : std::string();
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

void PasswordChangeSuccessTrackerImpl::OnChangePasswordFlowStarted(
    const GURL& url,
    const std::string& username,
    StartEvent event_type,
    EntryPoint entry_point) {
  ListPrefUpdate update(pref_service_,
                        prefs::kPasswordChangeSuccessTrackerFlows);
  base::Value::List& flows = update->GetList();
  RemoveFlowsWithTimeout(flows);

  flows.Append(base::Value(CreateFlow(ExtractEtld1(url), username, event_type,
                                      entry_point, base::Time::Now())));
}

void PasswordChangeSuccessTrackerImpl::OnManualChangePasswordFlowStarted(
    const GURL& url,
    const std::string& username,
    EntryPoint entry_point) {
  RemoveIncompleteFlowsWithTimeout();
  incomplete_manual_flows_.emplace_back(ExtractEtld1(url), username,
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
  auto predicate =
      [target_etld1{ExtractEtld1(url)}](const IncompleteFlow& flow) {
        return flow.etld1 == target_etld1;
      };
  if (auto it = std::find_if(incomplete_manual_flows_.cbegin(),
                             incomplete_manual_flows_.cend(), predicate);
      it != incomplete_manual_flows_.cend()) {
    ListPrefUpdate update(pref_service_,
                          prefs::kPasswordChangeSuccessTrackerFlows);
    base::Value::List& flows = update->GetList();
    RemoveFlowsWithTimeout(flows);

    flows.Append(base::Value(CreateFlow(it->etld1, it->username, new_event_type,
                                        it->entry_point, it->start_time)));
    incomplete_manual_flows_.erase(it);
  }
}

void PasswordChangeSuccessTrackerImpl::OnChangePasswordFlowModified(
    const GURL& url,
    const std::string& username,
    StartEvent new_event_type) {
  ListPrefUpdate update(pref_service_,
                        prefs::kPasswordChangeSuccessTrackerFlows);
  base::Value::List& flows = update->GetList();
  RemoveFlowsWithTimeout(flows);

  // Currently, this method can only get called if a request link is requested
  // inside an automated flow.
  DCHECK(new_event_type == StartEvent::kManualResetLinkFlow);

  // In the unlikely case that there are two flows with the same url and
  // username, we take the last entry.
  std::string target_etld1 = ExtractEtld1(url);
  for (size_t i = flows.size(); i-- > 0;) {
    FlowView view(&flows[i].GetDict());
    if (view.GetStartEvent() == StartEvent::kAutomatedFlow &&
        view.GetEtld1() == target_etld1 && view.GetUsername() == username) {
      EntryPoint entry_point = view.GetEntryPoint();
      RecordMetrics(view.GetEtld1(), view.GetStartEvent(),
                    EndEvent::kAutomatedFlowResetLinkRequestRequested,
                    entry_point, base::Time::Now() - view.GetStartTime());
      flows.erase(flows.begin() + i);

      // Add a new flow and reset the timer.
      flows.Append(base::Value(CreateFlow(target_etld1, username,
                                          StartEvent::kManualResetLinkFlow,
                                          entry_point, base::Time::Now())));

      return;
    }
  }
}

void PasswordChangeSuccessTrackerImpl::OnChangePasswordFlowCompleted(
    const GURL& url,
    const std::string& username,
    EndEvent event_type) {
  // If there are no ongoing change flows, return immediately to avoid disk
  // writes.
  const base::Value* read_flows =
      pref_service_->GetList(prefs::kPasswordChangeSuccessTrackerFlows);
  if (!read_flows || read_flows->GetList().empty())
    return;

  ListPrefUpdate update(pref_service_,
                        prefs::kPasswordChangeSuccessTrackerFlows);
  base::Value::List& flows = update->GetList();
  RemoveFlowsWithTimeout(flows);

  // In the unlikely case that there are two flows with the same eTLD+1 and
  // username, we take the last entry. The underlying assumption is that
  // the first flow was abandoned but has not timed out yet.
  std::string target_etld1 = ExtractEtld1(url);
  for (size_t i = flows.size(); i-- > 0;) {
    FlowView view(&flows[i].GetDict());
    if (view.GetEtld1() == target_etld1 && view.GetUsername() == username) {
      RecordMetrics(view.GetEtld1(), view.GetStartEvent(), event_type,
                    view.GetEntryPoint(),
                    base::Time::Now() - view.GetStartTime());
      flows.erase(flows.begin() + i);
      return;
    }
  }
}

void PasswordChangeSuccessTrackerImpl::AddMetricsRecorder(
    std::unique_ptr<PasswordChangeMetricsRecorder> recorder) {
  metrics_recorders_.push_back(std::move(recorder));
}

std::string PasswordChangeSuccessTrackerImpl::ExtractEtld1(const GURL& url) {
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
      RecordMetrics(view.GetEtld1(), view.GetStartEvent(), EndEvent::kTimeout,
                    view.GetEntryPoint(), kFlowTimeout);
      it = flows.erase(it);
    } else {
      // Flows are expected to be ordered by their time of creation.
      break;
    }
  }
}

void PasswordChangeSuccessTrackerImpl::RecordMetrics(const std::string& etld1,
                                                     StartEvent start_event,
                                                     EndEvent end_event,
                                                     EntryPoint entry_point,
                                                     base::TimeDelta duration) {
  for (const auto& recorder : metrics_recorders_) {
    recorder->OnFlowRecorded(etld1, start_event, end_event, entry_point,
                             duration);
  }
}

}  // namespace password_manager
