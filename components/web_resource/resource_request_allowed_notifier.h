// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_RESOURCE_RESOURCE_REQUEST_ALLOWED_NOTIFIER_H_
#define COMPONENTS_WEB_RESOURCE_RESOURCE_REQUEST_ALLOWED_NOTIFIER_H_

#include <memory>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "components/web_resource/eula_accepted_notifier.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class PrefService;

namespace web_resource {

// This class informs an interested observer when resource requests over the
// network are permitted.
//
// Currently, the criteria for allowing resource requests are:
//  1. The network is currently available,
//  2. The EULA was accepted by the user (can be disabled), and
//  3. The --disable-background-networking command line switch is not set.
//
// Interested services should add themselves as an observer of
// ResourceRequestAllowedNotifier and check ResourceRequestsAllowed() to see if
// requests are permitted. If it returns true, they can go ahead and make their
// request. If it returns false, ResourceRequestAllowedNotifier will notify the
// service when the criteria is met.
//
// If ResourceRequestsAllowed returns true the first time,
// ResourceRequestAllowedNotifier will not notify the service in the future.
//
// Note that this class handles the criteria state for a single service, so
// services should keep their own instance of this class rather than sharing a
// global instance.
class ResourceRequestAllowedNotifier
    : public EulaAcceptedNotifier::Observer,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  // Observes resource request allowed state changes.
  class Observer {
   public:
    virtual void OnResourceRequestsAllowed() = 0;
    virtual ~Observer() = default;
  };

  // Specifies the resource request allowed state.
  enum State {
    ALLOWED,
    DISALLOWED_EULA_NOT_ACCEPTED,
    DISALLOWED_NETWORK_DOWN,
    DISALLOWED_COMMAND_LINE_DISABLED,
    DISALLOWED_NETWORK_STATE_NOT_INITIALIZED,
  };

  using NetworkConnectionTrackerGetter =
      base::OnceCallback<network::NetworkConnectionTracker*()>;

  // Creates a new ResourceRequestAllowedNotifier.
  // |local_state| is the PrefService to observe.
  // |disable_network_switch| is the command line switch to disable network
  // activity. It is expected to outlive the ResourceRequestAllowedNotifier and
  // may be null.
  ResourceRequestAllowedNotifier(
      PrefService* local_state,
      const char* disable_network_switch,
      NetworkConnectionTrackerGetter network_connection_tracker_getter);

  ResourceRequestAllowedNotifier(const ResourceRequestAllowedNotifier&) =
      delete;
  ResourceRequestAllowedNotifier& operator=(
      const ResourceRequestAllowedNotifier&) = delete;

  ~ResourceRequestAllowedNotifier() override;

  // Sets |observer| as the service to be notified by this instance, and
  // performs initial checks on the criteria. |observer| may not be null.
  // This is to be called immediately after construction of an instance of
  // ResourceRequestAllowedNotifier to pass it the interested service. Set
  // |leaky| to true if this class will not be destructed before shutdown.
  void Init(Observer* observer, bool leaky);

  // Performs the same initialization as above, except that it allows for
  // disabling waiting for acceptance of EULA (by default it's enabled)
  void Init(Observer* observer, bool leaky, bool wait_for_eula);

  // Returns whether resource requests are allowed, per the various criteria.
  // If not, this call will set some flags so it knows to notify the observer
  // if the criteria change. Note that the observer will not be notified unless
  // it calls this method first.
  // This is virtual so it can be overridden for tests.
  virtual State GetResourceRequestsAllowedState();

  // Convenience function, equivalent to:
  //   GetResourceRequestsAllowedState() == ALLOWED.
  bool ResourceRequestsAllowed();

  void SetWaitingForEulaForTesting(bool waiting);
  void SetObserverRequestedForTesting(bool requested);
  void SetConnectionTypeForTesting(
      network::mojom::ConnectionType connection_type);

 protected:
  // Notifies the observer if all criteria needed for resource requests are met.
  // This is protected so it can be called from subclasses for testing.
  void MaybeNotifyObserver();

 private:
  // Creates the EulaAcceptNotifier or null if one is not needed. Virtual so
  // that it can be overridden by test subclasses.
  virtual EulaAcceptedNotifier* CreateEulaNotifier();

  // EulaAcceptedNotifier::Observer overrides:
  void OnEulaAccepted() override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver overrides:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  void SetConnectionType(network::mojom::ConnectionType connection_type);
  bool IsOffline();

  // Name of the command line switch to disable the network activity.
  const char* disable_network_switch_;

  // The local state this class is observing.
  raw_ptr<PrefService> local_state_;

  // Tracks whether or not the observer/service depending on this class actually
  // requested permission to make a request or not. If it did not, then this
  // class should not notify it even if the criteria is met.
  bool observer_requested_permission_;

  // Tracks EULA acceptance criteria.
  bool waiting_for_user_to_accept_eula_;

  // Platform-specific notifier of EULA acceptance, or null if not needed.
  std::unique_ptr<EulaAcceptedNotifier> eula_notifier_;

  // Observing service interested in request permissions.
  raw_ptr<Observer> observer_;

  NetworkConnectionTrackerGetter network_connection_tracker_getter_;
  raw_ptr<network::NetworkConnectionTracker> network_connection_tracker_ =
      nullptr;
  network::mojom::ConnectionType connection_type_ =
      network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  bool connection_initialized_ = false;

  base::WeakPtrFactory<ResourceRequestAllowedNotifier> weak_factory_{this};
};

BASE_DECLARE_FEATURE(kResourceRequestAllowedMigration);

}  // namespace web_resource

#endif  // COMPONENTS_WEB_RESOURCE_RESOURCE_REQUEST_ALLOWED_NOTIFIER_H_
