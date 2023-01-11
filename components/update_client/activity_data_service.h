// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_ACTIVITY_DATA_SERVICE_H_
#define COMPONENTS_UPDATE_CLIENT_ACTIVITY_DATA_SERVICE_H_

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"

namespace update_client {

const int kDateFirstTime = -1;
const int kDaysFirstTime = -1;
const int kDateUnknown = -2;
const int kDaysUnknown = -2;

// This is an interface that injects certain update information (active, days
// since ...) into the update engine of the update client.
// GetDaysSinceLastActive and GetDaysSinceLastRollCall are used for backward
// compatibility.
class ActivityDataService {
 public:
  // Calls `callback` with the subset of `ids` that are active.
  // The callback is called on the same sequence that calls this function.
  virtual void GetActiveBits(
      const std::vector<std::string>& ids,
      base::OnceCallback<void(const std::set<std::string>&)> callback)
      const = 0;

  // Calls `callback` with the subset of `ids` that are active, after clearing
  // their active setting.  The callback is called on the same sequence that
  // calls this function.
  virtual void GetAndClearActiveBits(
      const std::vector<std::string>& ids,
      base::OnceCallback<void(const std::set<std::string>&)> callback) = 0;

  // The following 2 functions return the number of days since last
  // active/roll call.
  virtual int GetDaysSinceLastActive(const std::string& id) const = 0;
  virtual int GetDaysSinceLastRollCall(const std::string& id) const = 0;

  virtual ~ActivityDataService() = default;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_ACTIVITY_DATA_SERVICE_H_
