// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_resource/resource_request_allowed_notifier.h"

#include "base/bind.h"
#include "base/command_line.h"

namespace web_resource {

ResourceRequestAllowedNotifier::ResourceRequestAllowedNotifier(
    PrefService* local_state,
    const char* disable_network_switch,
    NetworkConnectionTrackerGetter network_connection_tracker_getter)
    : disable_network_switch_(disable_network_switch),
      local_state_(local_state),
      observer_requested_permission_(false),
      waiting_for_user_to_accept_eula_(false),
      observer_(nullptr),
      network_connection_tracker_getter_(
          std::move(network_connection_tracker_getter)) {}

ResourceRequestAllowedNotifier::~ResourceRequestAllowedNotifier() {
  if (observer_)
    network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

void ResourceRequestAllowedNotifier::Init(Observer* observer, bool leaky) {
  DCHECK(!observer_);
  DCHECK(observer);
  observer_ = observer;

  DCHECK(network_connection_tracker_getter_);
  network_connection_tracker_ =
      std::move(network_connection_tracker_getter_).Run();

  if (leaky)
    network_connection_tracker_->AddLeakyNetworkConnectionObserver(this);
  else
    network_connection_tracker_->AddNetworkConnectionObserver(this);
  if (network_connection_tracker_->GetConnectionType(
          &connection_type_,
          base::BindOnce(&ResourceRequestAllowedNotifier::SetConnectionType,
                         weak_factory_.GetWeakPtr()))) {
    connection_initialized_ = true;
  }

  eula_notifier_.reset(CreateEulaNotifier());
  if (eula_notifier_) {
    eula_notifier_->Init(this);
    waiting_for_user_to_accept_eula_ = !eula_notifier_->IsEulaAccepted();
  }
}

ResourceRequestAllowedNotifier::State
ResourceRequestAllowedNotifier::GetResourceRequestsAllowedState() {
  if (disable_network_switch_ &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          disable_network_switch_)) {
    return DISALLOWED_COMMAND_LINE_DISABLED;
  }

  // The observer requested permission. Return the current criteria state and
  // set a flag to remind this class to notify the observer once the criteria
  // is met.
  observer_requested_permission_ =
      waiting_for_user_to_accept_eula_ || IsOffline();
  if (!observer_requested_permission_)
    return ALLOWED;
  if (waiting_for_user_to_accept_eula_)
    return DISALLOWED_EULA_NOT_ACCEPTED;
  if (!connection_initialized_)
    return DISALLOWED_NETWORK_STATE_NOT_INITIALIZED;
  return DISALLOWED_NETWORK_DOWN;
}

bool ResourceRequestAllowedNotifier::IsOffline() {
  return !connection_initialized_ ||
         connection_type_ == network::mojom::ConnectionType::CONNECTION_NONE;
}

bool ResourceRequestAllowedNotifier::ResourceRequestsAllowed() {
  return GetResourceRequestsAllowedState() == ALLOWED;
}

void ResourceRequestAllowedNotifier::SetWaitingForEulaForTesting(bool waiting) {
  waiting_for_user_to_accept_eula_ = waiting;
}

void ResourceRequestAllowedNotifier::SetObserverRequestedForTesting(
    bool requested) {
  observer_requested_permission_ = requested;
}

void ResourceRequestAllowedNotifier::SetConnectionTypeForTesting(
    network::mojom::ConnectionType type) {
  SetConnectionType(type);
}

void ResourceRequestAllowedNotifier::MaybeNotifyObserver() {
  // Need to ensure that all criteria are met before notifying observers.
  if (observer_requested_permission_ && ResourceRequestsAllowed()) {
    DVLOG(1) << "Notifying observer of state change.";
    observer_->OnResourceRequestsAllowed();
    // Reset this so the observer is not informed again unless they check
    // ResourceRequestsAllowed again.
    observer_requested_permission_ = false;
  }
}

EulaAcceptedNotifier* ResourceRequestAllowedNotifier::CreateEulaNotifier() {
  return EulaAcceptedNotifier::Create(local_state_);
}

void ResourceRequestAllowedNotifier::OnEulaAccepted() {
  // This flag should have been set if this was waiting on the EULA
  // notification.
  DCHECK(waiting_for_user_to_accept_eula_);
  DVLOG(1) << "EULA was accepted.";
  waiting_for_user_to_accept_eula_ = false;
  MaybeNotifyObserver();
}

void ResourceRequestAllowedNotifier::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  SetConnectionType(type);
  if (type != network::mojom::ConnectionType::CONNECTION_NONE) {
    DVLOG(1) << "Network came online.";
    // MaybeNotifyObserver() internally guarantees that it will only notify the
    // observer if it's currently waiting for the network to come online.
    MaybeNotifyObserver();
  }
}

void ResourceRequestAllowedNotifier::SetConnectionType(
    network::mojom::ConnectionType connection_type) {
  connection_type_ = connection_type;
  if (!connection_initialized_) {
    connection_initialized_ = true;
    MaybeNotifyObserver();
  }
}

}  // namespace web_resource
