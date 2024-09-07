// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/throttle/throttle_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"

namespace ash {

ThrottleService::ThrottleService(content::BrowserContext* context)
    : context_(context) {}

ThrottleService::~ThrottleService() = default;

void ThrottleService::AddServiceObserver(ServiceObserver* observer) {
  service_observers_.AddObserver(observer);
}

void ThrottleService::RemoveServiceObserver(ServiceObserver* observer) {
  service_observers_.RemoveObserver(observer);
}

ThrottleObserver* ThrottleService::GetObserverByName(const std::string& name) {
  for (auto& observer : observers_) {
    if (observer->name() == name)
      return observer.get();
  }
  return nullptr;
}

void ThrottleService::NotifyObserverStateChangedForTesting() {
  OnObserverStateChanged(nullptr);
}

void ThrottleService::SetObserversForTesting(
    std::vector<std::unique_ptr<ThrottleObserver>> observers) {
  StopObservers();
  observers_ = std::move(observers);
  StartObservers();
}

bool ThrottleService::HasServiceObserverForTesting(ServiceObserver* candidate) {
  return service_observers_.HasObserver(candidate);
}

void ThrottleService::AddObserver(std::unique_ptr<ThrottleObserver> observer) {
  observers_.push_back(std::move(observer));
}

void ThrottleService::StartObservers() {
  auto callback = base::BindRepeating(&ThrottleService::OnObserverStateChanged,
                                      weak_ptr_factory_.GetWeakPtr());
  for (auto& observer : observers_)
    observer->StartObserving(context_, callback);
}

void ThrottleService::StopObservers() {
  for (auto& observer : observers_)
    observer->StopObserving();
}

void ThrottleService::OnObserverStateChanged(
    const ThrottleObserver* changed_observer) {
  DVLOG(1) << "OnObserverStateChanged: changed throttle observer is "
           << (changed_observer ? changed_observer->name() : "none");

  ThrottleObserver* effective_observer = nullptr;

  bool should_throttle = true;
  // Check if there's an enforcing observer.
  for (auto& observer : observers_) {
    if (!observer->enforced())
      continue;
    DVLOG(1) << "Enforcing ThrottleObserver is found: name=" << observer->name()
             << ", active=" << observer->active();
    should_throttle = !observer->active();
    effective_observer = observer.get();
    break;
  }

  if (!effective_observer) {
    // No enforcing observer is found. Check if there are one (or more) active
    // observer(s).
    for (auto& observer : observers_) {
      if (!observer->active())
        continue;
      DVLOG(1) << "Active ThrottleObserver is found: name=" << observer->name();
      should_throttle = false;
      if (!effective_observer)
        effective_observer = observer.get();
      // Do not break; here to LOG all active observers. Treat the first one as
      // an effective observer.
    }
    if (!effective_observer)
      DVLOG(1) << "All ThrottleObserver(s) are inactive";
  }

  if (effective_observer != last_effective_observer_) {
    // If there is a new effective observer, record the duration that the last
    // effective observer was active.
    if (last_effective_observer_) {
      RecordCpuRestrictionDisabledUMA(
          last_effective_observer_->name(),
          base::TimeTicks::Now() - last_throttle_transition_);
    }
    last_throttle_transition_ = base::TimeTicks::Now();
    last_effective_observer_ = effective_observer;
  }

  if (should_throttle_ && (*should_throttle_ == should_throttle))
    return;

  // Do the actual throttling.
  should_throttle_ = should_throttle;
  ThrottleInstance(*should_throttle_);

  for (auto& observer : service_observers_)
    observer.OnThrottle(*should_throttle_);
}

}  // namespace ash
