// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_AUDIO_FAKE_FLOSS_MEDIA_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_AUDIO_FAKE_FLOSS_MEDIA_CLIENT_H_

#include <stdint.h>

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/audio/floss_media_client.h"

namespace ash {

// The FlossMediaClient implementation used on Linux desktop.
class COMPONENT_EXPORT(DBUS_AUDIO) FakeFlossMediaClient
    : public FlossMediaClient {
 public:
  FakeFlossMediaClient();

  FakeFlossMediaClient(const FakeFlossMediaClient&) = delete;
  FakeFlossMediaClient& operator=(const FakeFlossMediaClient&) = delete;

  ~FakeFlossMediaClient() override;

  static FakeFlossMediaClient* Get();

  void SetPlayerPlaybackStatus(const std::string& playback_status) override;
  void SetPlayerIdentity(const std::string& playback_identity) override;
  void SetPlayerPosition(const int64_t& position) override;
  void SetPlayerDuration(const int64_t& duration) override;
  void SetPlayerMetadata(
      const std::map<std::string, std::string>& metadata) override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_AUDIO_FAKE_FLOSS_MEDIA_CLIENT_H_
