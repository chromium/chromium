// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_GNUBBY_GNUBBY_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_GNUBBY_GNUBBY_CLIENT_H_

#include "base/component_export.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/u2f/dbus-constants.h"

namespace ash {
// GnubbyClient is used to communicate with the Gnubby service.
class COMPONENT_EXPORT(ASH_DBUS_GNUBBY) GnubbyClient
    : public chromeos::DBusClient {
 public:
  // Interface for observing changes in Gnubby Client
  class Observer {
   public:
    // Called when U2F service is requested
    virtual void PromptUserAuth() {}

   protected:
    virtual ~Observer() = default;
  };

  // Returns the global instance if initialized. May return null.
  static GnubbyClient* Get();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  GnubbyClient(const GnubbyClient&) = delete;
  GnubbyClient& operator=(const GnubbyClient&) = delete;

  // Adds an observer.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer if added.
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  // Initialize() should be used instead.
  GnubbyClient();
  ~GnubbyClient() override;
};
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_GNUBBY_GNUBBY_CLIENT_H_
