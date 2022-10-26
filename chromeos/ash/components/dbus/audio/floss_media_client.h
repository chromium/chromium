// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_AUDIO_FLOSS_MEDIA_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_AUDIO_FLOSS_MEDIA_CLIENT_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/component_export.h"
#include "dbus/object_path.h"

namespace dbus {
class Bus;
}

namespace ash {

// FlossMediaClient is used to communicate with the Floss media interface.
class COMPONENT_EXPORT(DBUS_AUDIO) FlossMediaClient {
 public:
  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus, const dbus::ObjectPath& object_path);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static FlossMediaClient* Get();

  FlossMediaClient(const FlossMediaClient&) = delete;
  FlossMediaClient& operator=(const FlossMediaClient&) = delete;

  // Sets the player playback status. Possible status are "Playing", "Paused" or
  // "Stopped".
  virtual void SetPlayerPlaybackStatus(const std::string& playback_status) = 0;

  // Sets the player identity. Identity is a human readable title for the source
  // of the media player. This could be the name of the app or the name of the
  // site playing media.
  virtual void SetPlayerIdentity(const std::string& playback_identity) = 0;

  // Sets the current track position for the player in microseconds
  virtual void SetPlayerPosition(const int64_t& position) = 0;

  // Sets the current track duration for the player in microseconds
  virtual void SetPlayerDuration(const int64_t& duration) = 0;

  // Sets the current media metadata including Title, Album, and Artist.
  virtual void SetPlayerMetadata(
      const std::map<std::string, std::string>& metadata) = 0;

 protected:
  FlossMediaClient();
  virtual ~FlossMediaClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_AUDIO_FLOSS_MEDIA_CLIENT_H_
