// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_GNUBBY_CLIENT_H_
#define CHROMEOS_DBUS_GNUBBY_CLIENT_H_

#include <memory>

#include "base/component_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/u2f/dbus-constants.h"

namespace chromeos {
// GnubbyClient is used to communicate with the Gnubby service.
class COMPONENT_EXPORT(CHROMEOS_DBUS) GnubbyClient : public DBusClient {
 public:
  // Interface for observing changes in Gnubby Client
  class Observer {
   public:
    // Called when U2F service is requested
    virtual void PromptUserAuth() {}

   protected:
    virtual ~Observer() = default;
  };

  // Create should be called instead of constructor.
  GnubbyClient();
  ~GnubbyClient() override;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static std::unique_ptr<GnubbyClient> Create();

  // Adds an observer.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer if added.
  virtual void RemoveObserver(Observer* observer) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GnubbyClient);
};
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_GNUBBY_CLIENT_H_
