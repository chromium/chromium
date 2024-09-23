// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_INTERACTIVE_CELLULAR_ESIM_NAME_OBSERVER_H_
#define CHROME_TEST_BASE_ASH_INTERACTIVE_CELLULAR_ESIM_NAME_OBSERVER_H_

#include <string>

#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "dbus/property.h"
#include "ui/base/interaction/state_observer.h"

namespace ash {

// This is a helper class that can be used in tests that use Kombucha to
// observe the eSIM network name changes.
class EsimNameObserver
    : public ui::test::ObservationStateObserver<std::string,
                                                HermesProfileClient,
                                                HermesProfileClient::Observer> {
 public:
  explicit EsimNameObserver(const dbus::ObjectPath object_path);
  ~EsimNameObserver() override;

 private:
  // HermesProfileClient::Observer:
  void OnCarrierProfilePropertyChanged(
      const dbus::ObjectPath& object_path,
      const std::string& property_name) override;

  // ui::test::ObservationStateObserver:
  std::string GetStateObserverInitialState() const override;

  std::string GetNetworkDisplayName() const;

  const dbus::ObjectPath object_path_;
};

}  // namespace ash

#endif  // CHROME_TEST_BASE_ASH_INTERACTIVE_CELLULAR_ESIM_NAME_OBSERVER_H_
