// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MACHINE_LEARNING_MACHINE_LEARNING_CLIENT_H_
#define CHROMEOS_DBUS_MACHINE_LEARNING_MACHINE_LEARNING_CLIENT_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/files/scoped_file.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// D-Bus client for ML service. Its only purpose is to bootstrap a Mojo
// connection to the ML service daemon.
class COMPONENT_EXPORT(MACHINE_LEARNING) MachineLearningClient {
 public:
  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static MachineLearningClient* Get();

  // Passes the file descriptor |fd| over D-Bus to the ML service daemon.
  // * The daemon expects a Mojo invitation in |fd| with an attached Mojo pipe.
  // * The daemon will bind the Mojo pipe to an implementation of
  //   chromeos::machine_learning::mojom::MachineLearningService.
  // * Upon completion of the D-Bus call, |result_callback| will be invoked to
  //   indicate success or failure.
  // * This method will first wait for the ML service to become available.
  virtual void BootstrapMojoConnection(
      base::ScopedFD fd,
      base::OnceCallback<void(bool success)> result_callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  MachineLearningClient();
  virtual ~MachineLearningClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(MachineLearningClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MACHINE_LEARNING_MACHINE_LEARNING_CLIENT_H_
