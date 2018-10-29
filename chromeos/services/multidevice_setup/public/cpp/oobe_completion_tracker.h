// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_OOBE_COMPLETION_TRACKER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_OOBE_COMPLETION_TRACKER_H_

#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"

namespace chromeos {

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
  ~OobeCompletionTracker() override;

  void MarkOobeShown();

 private:
  base::ObserverList<Observer>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(OobeCompletionTracker);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_OOBE_COMPLETION_TRACKER_H_
