// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MISSIVE_MISSIVE_CLIENT_H_
#define CHROMEOS_DBUS_MISSIVE_MISSIVE_CLIENT_H_

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/weak_ptr.h"
#include "components/reporting/storage/missive_storage_module.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// D-Bus client for Missive service.
// Missive service provides a method for enterprise customers to locally encrypt
// and store |reporting::Record|s.
class COMPONENT_EXPORT(MISSIVE) MissiveClient {
 public:
  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static MissiveClient* Get();

  scoped_refptr<reporting::MissiveStorageModule> GetMissiveStorageModule();

 protected:
  // Initialize/Shutdown should be used instead.
  MissiveClient();
  virtual ~MissiveClient();

  scoped_refptr<reporting::MissiveStorageModule> missive_storage_module_;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
using ::chromeos::MissiveClient;
}  // namespace ash

#endif  // CHROMEOS_DBUS_MISSIVE_MISSIVE_CLIENT_H_
