// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_HIBERMAN_FAKE_HIBERMAN_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_HIBERMAN_FAKE_HIBERMAN_CLIENT_H_

#include "chromeos/ash/components/dbus/hiberman/hiberman_client.h"

#include <vector>

#include "base/component_export.h"

namespace ash {

// Class which satisfies the implementation of a HibermanClient but does not
// actually wire up to dbus. Used in testing.
class COMPONENT_EXPORT(HIBERMAN_CLIENT) FakeHibermanClient
    : public HibermanClient {
 public:
  FakeHibermanClient();
  ~FakeHibermanClient() override;

  // Not copyable or movable.
  FakeHibermanClient(const FakeHibermanClient&) = delete;
  FakeHibermanClient& operator=(const FakeHibermanClient&) = delete;

  // Checks that a FakeHibermanClient instance was initialized and returns
  // it.
  static FakeHibermanClient* Get();

  // HibermanClient override:
  bool IsAlive() const override;
  bool IsEnabled() const override;
  bool IsHibernateToS4Enabled() const override;
  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override;
  void ResumeFromHibernate(const std::string& account_id,
                           const std::string& auth_session_id) override;
  void AbortResumeHibernate(const std::string& reason) override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_HIBERMAN_FAKE_HIBERMAN_CLIENT_H_
