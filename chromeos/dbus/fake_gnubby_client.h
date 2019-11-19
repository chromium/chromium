// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_GNUBBY_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_GNUBBY_CLIENT_H_

#include "base/observer_list.h"
#include "chromeos/dbus/gnubby_client.h"

namespace chromeos {

// A fake implementation of GnubbyClient used for tests.
class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeGnubbyClient : public GnubbyClient {
 public:
  FakeGnubbyClient();
  ~FakeGnubbyClient() override;

  // GnubbyClient overrides
  void Init(dbus::Bus* bus) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void SignalPromptUserAuth();
  int calls() { return calls_; }

 private:
  int calls_ = 0;

  base::ObserverList<Observer>::Unchecked observer_list_;

  base::WeakPtrFactory<FakeGnubbyClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeGnubbyClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_GNUBBY_CLIENT_H_
