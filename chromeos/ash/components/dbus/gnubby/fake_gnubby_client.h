// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_GNUBBY_FAKE_GNUBBY_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_GNUBBY_FAKE_GNUBBY_CLIENT_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/gnubby/gnubby_client.h"

namespace ash {

// A fake implementation of GnubbyClient used for tests.
class COMPONENT_EXPORT(ASH_DBUS_GNUBBY) FakeGnubbyClient : public GnubbyClient {
 public:
  FakeGnubbyClient();

  FakeGnubbyClient(const FakeGnubbyClient&) = delete;
  FakeGnubbyClient& operator=(const FakeGnubbyClient&) = delete;

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
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_GNUBBY_FAKE_GNUBBY_CLIENT_H_
