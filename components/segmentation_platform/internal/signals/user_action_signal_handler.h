// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_USER_ACTION_SIGNAL_HANDLER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_USER_ACTION_SIGNAL_HANDLER_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"

namespace segmentation_platform {

class SignalDatabase;

// Responsible for listening to user action events and persisting it to the
// internal database for future processing.
class UserActionSignalHandler {
 public:
  explicit UserActionSignalHandler(SignalDatabase* signal_database);
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

 private:
  void OnUserAction(const std::string& user_action,
                    base::TimeTicks action_time);

  // The database storing relevant user actions.
  raw_ptr<SignalDatabase> db_;

  // The callback registered with user metrics module that gets invoked for
  // every user action.
  base::ActionCallback action_callback_;

  // The set of user actions relevant to the segmentation platform. Everything
  // else will be filtered out.
  std::set<uint64_t> user_actions_;

  // Whether or not the segmentation platform should record metrics events.
  bool metrics_enabled_;

  base::WeakPtrFactory<UserActionSignalHandler> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_USER_ACTION_SIGNAL_HANDLER_H_
