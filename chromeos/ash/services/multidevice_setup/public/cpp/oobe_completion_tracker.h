// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_OOBE_COMPLETION_TRACKER_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_OOBE_COMPLETION_TRACKER_H_

#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"

namespace ash {

namespace multidevice_setup {

// Records if/when the user underwent the OOBE MultiDevice setup flow to prevent
// spamming the user with multiple notifications to set up MultiDevice features.
class OobeCompletionTracker : public KeyedService {
 public:
  class Observer {
   public:
    virtual void OnOobeCompleted() = 0;

   protected:
    virtual ~Observer() = default;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  OobeCompletionTracker();

  OobeCompletionTracker(const OobeCompletionTracker&) = delete;
  OobeCompletionTracker& operator=(const OobeCompletionTracker&) = delete;

  ~OobeCompletionTracker() override;

  void MarkOobeShown();

 private:
  base::ObserverList<Observer>::Unchecked observer_list_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_OOBE_COMPLETION_TRACKER_H_
