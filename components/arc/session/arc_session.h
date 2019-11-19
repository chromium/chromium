// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SESSION_ARC_SESSION_H_
#define COMPONENTS_ARC_SESSION_ARC_SESSION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/arc/session/arc_stop_reason.h"
#include "components/arc/session/arc_upgrade_params.h"

namespace ash {
class DefaultScaleFactorRetriever;
}

namespace chromeos {
class SchedulerConfigurationManagerBase;
}

namespace version_info {
enum class Channel;
}

namespace arc {

class ArcBridgeService;

// Starts the ARC instance and bootstraps the bridge connection.
// Clients should implement the Delegate to be notified upon communications
// being available.
// The instance can be safely removed before StartMiniInstance() is called, or
// after OnSessionStopped() is called.  The number of instances must be at
// most one. Otherwise, ARC instances will conflict.
class ArcSession {
 public:
  // Observer to notify events corresponding to one ARC session run.
  class Observer {
   public:
    // Called when ARC instance is stopped. This is called exactly once per
    // instance.  |was_running| is true if the stopped instance was fully set
    // up and running. |full_requested| is true if the full container was
    // requested.
    virtual void OnSessionStopped(ArcStopReason reason,
                                  bool was_running,
                                  bool full_requested) = 0;

   protected:
    virtual ~Observer() = default;
  };

  // Creates a default instance of ArcSession.
  static std::unique_ptr<ArcSession> Create(
      ArcBridgeService* arc_bridge_service,
      ash::DefaultScaleFactorRetriever* retriever,
      version_info::Channel channel,
      chromeos::SchedulerConfigurationManagerBase*
          scheduler_configuration_manager);
  virtual ~ArcSession();

  // Sends D-Bus message to start a mini-container.
  virtual void StartMiniInstance() = 0;

  // Sends a D-Bus message to upgrade to a full instance if possible. This
  // might be done asynchronously; the message might only be sent after other
  // operations have completed.
  virtual void RequestUpgrade(UpgradeParams params) = 0;

  // Requests to stop the currently-running instance regardless of its mode.
  // The completion is notified via OnSessionStopped() of the Observer.
  virtual void Stop() = 0;

  // Returns true if Stop() has been called already.
  virtual bool IsStopRequested() = 0;

  // Called when Chrome is in shutdown state. This is called when the message
  // loop is already stopped, and the instance will soon be deleted. Caller
  // may expect that OnSessionStopped() is synchronously called back except
  // when it has already been called before.
  virtual void OnShutdown() = 0;

  // Sets a hash string of the profile user ID and an ARC serial number for the
  // user.
  virtual void SetUserInfo(const std::string& hash,
                           const std::string& serial_number) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  ArcSession();

  base::ObserverList<Observer>::Unchecked observer_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcSession);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_ARC_SESSION_H_
