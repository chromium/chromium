// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_PROFILE_OBSERVER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_PROFILE_OBSERVER_H_

namespace ash {

struct NetworkProfile;

class NetworkProfileObserver {
 public:
  NetworkProfileObserver& operator=(const NetworkProfileObserver&) = delete;

  virtual void OnProfileAdded(const NetworkProfile& profile) = 0;
  virtual void OnProfileRemoved(const NetworkProfile& profile) = 0;

 protected:
  virtual ~NetworkProfileObserver() {}
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_PROFILE_OBSERVER_H_
