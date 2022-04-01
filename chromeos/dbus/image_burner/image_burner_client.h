// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IMAGE_BURNER_IMAGE_BURNER_CLIENT_H_
#define CHROMEOS_DBUS_IMAGE_BURNER_IMAGE_BURNER_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "chromeos/dbus/common/dbus_client.h"

namespace chromeos {

// ImageBurnerClient is used to communicate with the image burner.
// All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(CHROMEOS_DBUS_IMAGE_BURNER) ImageBurnerClient
    : public DBusClient {
 public:
  ImageBurnerClient(const ImageBurnerClient&) = delete;
  ImageBurnerClient& operator=(const ImageBurnerClient&) = delete;

  ~ImageBurnerClient() override;

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

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static std::unique_ptr<ImageBurnerClient> Create();

 protected:
  // Create() should be used instead.
  ImageBurnerClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IMAGE_BURNER_IMAGE_BURNER_CLIENT_H_
