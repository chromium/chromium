// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_USER_ACTION_SIGNAL_HANDLER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_USER_ACTION_SIGNAL_HANDLER_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/ukm_database.h"

namespace segmentation_platform {

class SignalDatabase;

// Responsible for listening to user action events and persisting it to the
// internal database for future processing.
class UserActionSignalHandler {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when a histogram signal tracked by segmentation platform is
    // updated and written to database.
    virtual void OnUserAction(const std::string& user_action,
                              base::TimeTicks action_time) = 0;
    ~Observer() override = default;

   protected:
    Observer() = default;
  };

  UserActionSignalHandler(const std::string& profile_id,
                          SignalDatabase* signal_database,
                          UkmDatabase* ukm_db);
  virtual ~UserActionSignalHandler();

  // Disallow copy/assign.
  UserActionSignalHandler(const UserActionSignalHandler&) = delete;
  UserActionSignalHandler& operator=(const UserActionSignalHandler&) = delete;

  // Called to notify about a set of user actions which the segmentation models
  // care about.
  virtual void SetRelevantUserActions(std::set<uint64_t> user_actions);

  // Called to enable or disable metrics collection for segmentation platform.
  // This can be called early even before relevant user actions are known.
  virtual void EnableMetrics(bool enable_metrics);

  // Add/Remove observer for histogram update events.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

 private:
  void OnUserAction(const std::string& user_action,
                    base::TimeTicks action_time);
  void OnSampleWritten(const std::string& user_action,
                       base::TimeTicks action_time,
                       bool success);

  const std::string profile_id_;

  // The database storing relevant user actions.
  const raw_ptr<SignalDatabase> db_;
  const raw_ptr<UkmDatabase> ukm_db_;

  // The callback registered with user metrics module that gets invoked for
  // every user action.
  base::ActionCallback action_callback_;

  // The set of user actions relevant to the segmentation platform. Everything
  // else will be filtered out.
  std::set<uint64_t> user_actions_;

  base::ObserverList<Observer> observers_;

  // Whether or not the segmentation platform should record metrics events.
  bool metrics_enabled_;

  base::WeakPtrFactory<UserActionSignalHandler> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_USER_ACTION_SIGNAL_HANDLER_H_
