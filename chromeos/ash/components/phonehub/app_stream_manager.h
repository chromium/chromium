// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_APP_STREAM_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_APP_STREAM_MANAGER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"

namespace ash {
namespace phonehub {

// Provides updates on the app stream.
class AppStreamManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnAppStreamUpdate(
        const proto::AppStreamUpdate app_stream_update) = 0;
  };

  AppStreamManager();

  AppStreamManager(const AppStreamManager&) = delete;
  AppStreamManager& operator=(const AppStreamManager&) = delete;
  virtual ~AppStreamManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  friend class PhoneStatusProcessor;

  void NotifyAppStreamUpdate(const proto::AppStreamUpdate app_stream_update);

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_APP_STREAM_MANAGER_H_
