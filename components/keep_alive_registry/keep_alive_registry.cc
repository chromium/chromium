// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keep_alive_registry/keep_alive_registry.h"

#include "base/logging.h"
#include "base/observer_list.h"
#include "components/keep_alive_registry/keep_alive_state_observer.h"
#include "components/keep_alive_registry/keep_alive_types.h"

////////////////////////////////////////////////////////////////////////////////
// Public methods

// static
KeepAliveRegistry* KeepAliveRegistry::GetInstance() {
  return base::Singleton<KeepAliveRegistry>::get();
}

bool KeepAliveRegistry::IsKeepingAlive() const {
  return registered_count_ > 0 && !(IsRestartAllowed() && is_restarting_);
}

bool KeepAliveRegistry::IsKeepingAliveOnlyByBrowserOrigin() const {
  for (const auto& value : registered_keep_alives_) {
    if (value.first != KeepAliveOrigin::BROWSER)
      return false;
  }
  return true;
}

bool KeepAliveRegistry::IsRestartAllowed() const {
  return registered_count_ == restart_allowed_count_;
}

bool KeepAliveRegistry::IsOriginRegistered(KeepAliveOrigin origin) const {
  return registered_keep_alives_.find(origin) != registered_keep_alives_.end();
}

void KeepAliveRegistry::AddObserver(KeepAliveStateObserver* observer) {
  observers_.AddObserver(observer);
}

void KeepAliveRegistry::RemoveObserver(KeepAliveStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool KeepAliveRegistry::WouldRestartWithout(
    const std::vector<KeepAliveOrigin>& origins) const {
  int registered_count = 0;
  int restart_allowed_count = 0;

  for (auto origin : origins) {
    auto counts_it = registered_keep_alives_.find(origin);
    if (counts_it != registered_keep_alives_.end()) {
      registered_count += counts_it->second;

      counts_it = restart_allowed_keep_alives_.find(origin);
      if (counts_it != restart_allowed_keep_alives_.end())
        restart_allowed_count += counts_it->second;
    } else {
      // |registered_keep_alives_| is supposed to be a superset of
      // |restart_allowed_keep_alives_|
      DCHECK(restart_allowed_keep_alives_.find(origin) ==
             restart_allowed_keep_alives_.end());
    }
  }

  registered_count = registered_count_ - registered_count;
  restart_allowed_count = restart_allowed_count_ - restart_allowed_count;

  DCHECK_GE(registered_count, 0);
  DCHECK_GE(restart_allowed_count, 0);

  return registered_count == restart_allowed_count;
}

bool KeepAliveRegistry::IsShuttingDown() const {
  return is_shutting_down_;
}

void KeepAliveRegistry::SetIsShuttingDown(bool value) {
  is_shutting_down_ = value;
}

bool KeepAliveRegistry::IsRestarting() const {
  return is_restarting_;
}

void KeepAliveRegistry::SetRestarting() {
  bool old_keeping_alive = IsKeepingAlive();
  is_restarting_ = true;
  bool new_keeping_alive = IsKeepingAlive();

  // keep alive state can be updated by |is_restarting_| change.
  // If that happens, notify observers.
  if (old_keeping_alive != new_keeping_alive)
    OnKeepAliveStateChanged(new_keeping_alive);
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

KeepAliveRegistry::KeepAliveRegistry()
    : registered_count_(0), restart_allowed_count_(0) {}

KeepAliveRegistry::~KeepAliveRegistry() {
  DCHECK_LE(registered_count_, 0) << "KeepAliveRegistry state:" << *this;
  DCHECK_EQ(registered_keep_alives_.size(), 0u)
      << "KeepAliveRegistry state:" << *this;
}

void KeepAliveRegistry::Register(KeepAliveOrigin origin,
                                 KeepAliveRestartOption restart) {
  CHECK(!is_shutting_down_);

  bool old_keeping_alive = IsKeepingAlive();
  bool old_restart_allowed = IsRestartAllowed();

  ++registered_keep_alives_[origin];
  ++registered_count_;

  if (restart == KeepAliveRestartOption::ENABLED) {
    ++restart_allowed_keep_alives_[origin];
    ++restart_allowed_count_;
  }

  bool new_keeping_alive = IsKeepingAlive();
  bool new_restart_allowed = IsRestartAllowed();

  if (new_keeping_alive != old_keeping_alive)
    OnKeepAliveStateChanged(new_keeping_alive);

  if (new_restart_allowed != old_restart_allowed)
    OnRestartAllowedChanged(new_restart_allowed);

  DVLOG(1) << "New state of the KeepAliveRegistry: " << *this;
}

void KeepAliveRegistry::Unregister(KeepAliveOrigin origin,
                                   KeepAliveRestartOption restart) {
  bool old_keeping_alive = IsKeepingAlive();
  bool old_restart_allowed = IsRestartAllowed();

  --registered_count_;
  DCHECK_GE(registered_count_, 0);
  DecrementCount(origin, &registered_keep_alives_);

  if (restart == KeepAliveRestartOption::ENABLED) {
    --restart_allowed_count_;
    DecrementCount(origin, &restart_allowed_keep_alives_);
  }

  bool new_keeping_alive = IsKeepingAlive();
  bool new_restart_allowed = IsRestartAllowed();

  // Update the KeepAlive state first, so that listeners can check if we are
  // trying to shutdown.
  if (new_keeping_alive != old_keeping_alive)
    OnKeepAliveStateChanged(new_keeping_alive);

  if (new_restart_allowed != old_restart_allowed)
    OnRestartAllowedChanged(new_restart_allowed);

  DVLOG(1) << "New state of the KeepAliveRegistry:" << *this;
}

void KeepAliveRegistry::OnKeepAliveStateChanged(bool new_keeping_alive) {
  DVLOG(1) << "Notifying KeepAliveStateObservers: KeepingAlive changed to: "
           << new_keeping_alive;
  for (KeepAliveStateObserver& observer : observers_)
    observer.OnKeepAliveStateChanged(new_keeping_alive);
}

void KeepAliveRegistry::OnRestartAllowedChanged(bool new_restart_allowed) {
  DVLOG(1) << "Notifying KeepAliveStateObservers: Restart changed to: "
           << new_restart_allowed;
  for (KeepAliveStateObserver& observer : observers_)
    observer.OnKeepAliveRestartStateChanged(new_restart_allowed);
}

void KeepAliveRegistry::DecrementCount(KeepAliveOrigin origin,
                                       OriginMap* keep_alive_map) {
  int new_count = --keep_alive_map->at(origin);
  DCHECK_GE(keep_alive_map->at(origin), 0);
  if (new_count == 0)
    keep_alive_map->erase(origin);
}

std::ostream& operator<<(std::ostream& out, const KeepAliveRegistry& registry) {
  out << "{registered_count_=" << registry.registered_count_
      << ", restart_allowed_count_=" << registry.restart_allowed_count_
      << ", KeepAlives=[";
  for (auto counts_per_origin_it : registry.registered_keep_alives_) {
    if (counts_per_origin_it != *registry.registered_keep_alives_.begin())
      out << ", ";
    out << counts_per_origin_it.first << " (" << counts_per_origin_it.second
        << ")";
  }
  out << "]}";
  return out;
}
