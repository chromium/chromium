// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/registration_manager.h"

#include <stdint.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/rand_util.h"
#include "base/stl_util.h"
#include "components/invalidation/public/invalidation_util.h"
#include "google/cacheinvalidation/include/invalidation-client.h"
#include "google/cacheinvalidation/include/types.h"

namespace syncer {

RegistrationManager::PendingRegistrationInfo::PendingRegistrationInfo() {}

RegistrationManager::RegistrationStatus::RegistrationStatus(
    const invalidation::ObjectId& id, RegistrationManager* manager)
    : id(id),
      registration_manager(manager),
      enabled(true),
      state(invalidation::InvalidationListener::UNREGISTERED) {
  DCHECK(registration_manager);
}

RegistrationManager::RegistrationStatus::~RegistrationStatus() {}

void RegistrationManager::RegistrationStatus::DoRegister() {
  CHECK(enabled);
  // We might be called explicitly, so stop the timer manually and
  // reset the delay.
  registration_timer.Stop();
  delay = base::TimeDelta();
  registration_manager->DoRegisterId(id);
  DCHECK(!last_registration_request.is_null());
}

void RegistrationManager::RegistrationStatus::Disable() {
  enabled = false;
  state = invalidation::InvalidationListener::UNREGISTERED;
  registration_timer.Stop();
  delay = base::TimeDelta();
}

const int RegistrationManager::kInitialRegistrationDelaySeconds = 5;
const int RegistrationManager::kRegistrationDelayExponent = 2;
const double RegistrationManager::kRegistrationDelayMaxJitter = 0.5;
const int RegistrationManager::kMinRegistrationDelaySeconds = 1;
// 1 hour.
const int RegistrationManager::kMaxRegistrationDelaySeconds = 60 * 60;

RegistrationManager::RegistrationManager(
    invalidation::InvalidationClient* invalidation_client)
    : invalidation_client_(invalidation_client) {
  DCHECK(invalidation_client_);
}

RegistrationManager::~RegistrationManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ObjectIdSet RegistrationManager::UpdateRegisteredIds(const ObjectIdSet& ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const ObjectIdSet& old_ids = GetRegisteredIds();
  const ObjectIdSet& to_register = ids;
  ObjectIdSet to_unregister;
  std::set_difference(old_ids.begin(), old_ids.end(),
                      ids.begin(), ids.end(),
                      std::inserter(to_unregister, to_unregister.begin()),
                      ObjectIdLessThan());

  for (auto it = to_unregister.begin(); it != to_unregister.end(); ++it) {
    UnregisterId(*it);
  }

  for (auto it = to_register.begin(); it != to_register.end(); ++it) {
    if (!base::Contains(registration_statuses_, *it)) {
      registration_statuses_[*it] =
          std::make_unique<RegistrationStatus>(*it, this);
    }
    if (!IsIdRegistered(*it)) {
      TryRegisterId(*it, false /* is-retry */);
    }
  }

  return to_unregister;
}

void RegistrationManager::MarkRegistrationLost(
    const invalidation::ObjectId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = registration_statuses_.find(id);
  if (it == registration_statuses_.end()) {
    DVLOG(1) << "Attempt to mark non-existent registration for "
             << ObjectIdToString(id) << " as lost";
    return;
  }
  if (!it->second->enabled) {
    return;
  }
  it->second->state = invalidation::InvalidationListener::UNREGISTERED;
  bool is_retry = !it->second->last_registration_request.is_null();
  TryRegisterId(id, is_retry);
}

void RegistrationManager::MarkAllRegistrationsLost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto it = registration_statuses_.begin();
       it != registration_statuses_.end(); ++it) {
    if (IsIdRegistered(it->first)) {
      MarkRegistrationLost(it->first);
    }
  }
}

void RegistrationManager::DisableId(const invalidation::ObjectId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = registration_statuses_.find(id);
  if (it == registration_statuses_.end()) {
    DVLOG(1) << "Attempt to disable non-existent registration for "
             << ObjectIdToString(id);
    return;
  }
  it->second->Disable();
}

// static
double RegistrationManager::CalculateBackoff(
    double retry_interval,
    double initial_retry_interval,
    double min_retry_interval,
    double max_retry_interval,
    double backoff_exponent,
    double jitter,
    double max_jitter) {
  // scaled_jitter lies in [-max_jitter, max_jitter].
  double scaled_jitter = jitter * max_jitter;
  double new_retry_interval =
      (retry_interval == 0.0) ?
      (initial_retry_interval * (1.0 + scaled_jitter)) :
      (retry_interval * (backoff_exponent + scaled_jitter));
  return std::max(min_retry_interval,
                  std::min(max_retry_interval, new_retry_interval));
}

ObjectIdSet RegistrationManager::GetRegisteredIdsForTest() const {
  return GetRegisteredIds();
}

RegistrationManager::PendingRegistrationMap
    RegistrationManager::GetPendingRegistrationsForTest() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PendingRegistrationMap pending_registrations;
  for (const auto& status_pair : registration_statuses_) {
    const invalidation::ObjectId& id = status_pair.first;
    RegistrationStatus* status = status_pair.second.get();
    if (status->registration_timer.IsRunning()) {
      pending_registrations[id].last_registration_request =
          status->last_registration_request;
      pending_registrations[id].registration_attempt =
          status->last_registration_attempt;
      pending_registrations[id].delay = status->delay;
      pending_registrations[id].actual_delay =
          status->registration_timer.GetCurrentDelay();
    }
  }
  return pending_registrations;
}

void RegistrationManager::FirePendingRegistrationsForTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& status_pair : registration_statuses_) {
    if (status_pair.second->registration_timer.IsRunning()) {
      status_pair.second->DoRegister();
    }
  }
}

double RegistrationManager::GetJitter() {
  // |jitter| lies in [-1.0, 1.0), which is low-biased, but only
  // barely.
  //
  // TODO(akalin): Fix the bias.
  return 2.0 * base::RandDouble() - 1.0;
}

void RegistrationManager::TryRegisterId(const invalidation::ObjectId& id,
                                        bool is_retry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = registration_statuses_.find(id);
  if (it == registration_statuses_.end()) {
    NOTREACHED() << "TryRegisterId called on " << ObjectIdToString(id)
                 << " which is not in the registration map";
    return;
  }
  RegistrationStatus* status = it->second.get();
  if (!status->enabled) {
    // Disabled, so do nothing.
    return;
  }
  status->last_registration_attempt = base::Time::Now();
  if (is_retry) {
    // If we're a retry, we must have tried at least once before.
    DCHECK(!status->last_registration_request.is_null());
    // delay = max(0, (now - last request) + next_delay)
    status->delay =
        (status->last_registration_request -
         status->last_registration_attempt) +
        status->next_delay;
    base::TimeDelta delay =
        (status->delay <= base::TimeDelta()) ?
        base::TimeDelta() : status->delay;
    DVLOG(2) << "Registering "
             << ObjectIdToString(id) << " in "
             << delay.InMilliseconds() << " ms";
    status->registration_timer.Stop();
    status->registration_timer.Start(FROM_HERE,
        delay, status, &RegistrationManager::RegistrationStatus::DoRegister);
    double next_delay_seconds =
        CalculateBackoff(static_cast<double>(status->next_delay.InSeconds()),
                         kInitialRegistrationDelaySeconds,
                         kMinRegistrationDelaySeconds,
                         kMaxRegistrationDelaySeconds,
                         kRegistrationDelayExponent,
                         GetJitter(),
                         kRegistrationDelayMaxJitter);
    status->next_delay =
        base::TimeDelta::FromSeconds(static_cast<int64_t>(next_delay_seconds));
    DVLOG(2) << "New next delay for "
             << ObjectIdToString(id) << " is "
             << status->next_delay.InSeconds() << " seconds";
  } else {
    DVLOG(2) << "Not a retry -- registering "
             << ObjectIdToString(id) << " immediately";
    status->delay = base::TimeDelta();
    status->next_delay = base::TimeDelta();
    status->DoRegister();
  }
}

void RegistrationManager::DoRegisterId(const invalidation::ObjectId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  invalidation_client_->Register(id);
  auto it = registration_statuses_.find(id);
  if (it == registration_statuses_.end()) {
    NOTREACHED() << "DoRegisterId called on " << ObjectIdToString(id)
                 << " which is not in the registration map";
    return;
  }
  it->second->state = invalidation::InvalidationListener::REGISTERED;
  it->second->last_registration_request = base::Time::Now();
}

void RegistrationManager::UnregisterId(const invalidation::ObjectId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  invalidation_client_->Unregister(id);
  auto it = registration_statuses_.find(id);
  if (it == registration_statuses_.end()) {
    NOTREACHED() << "UnregisterId called on " << ObjectIdToString(id)
                 << " which is not in the registration map";
    return;
  }
  registration_statuses_.erase(it);
}


ObjectIdSet RegistrationManager::GetRegisteredIds() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ObjectIdSet ids;
  for (const auto& status_pair : registration_statuses_) {
    if (IsIdRegistered(status_pair.first)) {
      ids.insert(status_pair.first);
    }
  }
  return ids;
}

bool RegistrationManager::IsIdRegistered(
    const invalidation::ObjectId& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = registration_statuses_.find(id);
  return it != registration_statuses_.end() &&
      it->second->state == invalidation::InvalidationListener::REGISTERED;
}

}  // namespace syncer
