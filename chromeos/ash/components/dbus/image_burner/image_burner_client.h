// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_IMAGE_BURNER_IMAGE_BURNER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_IMAGE_BURNER_IMAGE_BURNER_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/dbus/common/dbus_client.h"

namespace ash {

// ImageBurnerClient is used to communicate with the image burner.
// All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(ASH_DBUS_IMAGE_BURNER) ImageBurnerClient
    : public chromeos::DBusClient {
 public:
  // Returns the global instance if initialized. May return null.
  static ImageBurnerClient* Get();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Sets a temporary instance for testing. Overrides the existing
  // global instance, if any.
  static void SetInstanceForTest(ImageBurnerClient* client);

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  ImageBurnerClient(const ImageBurnerClient&) = delete;
  ImageBurnerClient& operator=(const ImageBurnerClient&) = delete;

  // A callback to be called when DBus method call fails.
  using ErrorCallback = base::OnceCallback<void()>;

  // A callback to handle burn_finished signal.
  using BurnFinishedHandler =
      base::OnceCallback<void(const std::string& target_path,
                              bool success,
                              const std::string& error)>;

  // A callback to handle burn_progress_update signal.
  using BurnProgressUpdateHandler =
      base::RepeatingCallback<void(const std::string& target_path,
                                   int64_t num_bytes_burnt,
                                   int64_t total_size)>;

  // Burns the image |from_path| to the disk |to_path|.
  virtual void BurnImage(const std::string& from_path,
                         const std::string& to_path,
                         ErrorCallback error_callback) = 0;

  // Sets callbacks as event handlers.
  // |burn_finished_handler| is called when burn_finished signal is received.
  // |burn_progress_update_handler| is called when burn_progress_update signal
  // is received.
  virtual void SetEventHandlers(
      BurnFinishedHandler burn_finished_handler,
      const BurnProgressUpdateHandler& burn_progress_update_handler) = 0;

  // Resets event handlers. After calling this method, nothing is done when
  // signals are received.
  virtual void ResetEventHandlers() = 0;

 protected:
  // Initialize() should be used instead.
  ImageBurnerClient();
  ~ImageBurnerClient() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_IMAGE_BURNER_IMAGE_BURNER_CLIENT_H_
