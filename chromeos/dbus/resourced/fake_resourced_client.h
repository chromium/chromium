// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_RESOURCED_FAKE_RESOURCED_CLIENT_H_
#define CHROMEOS_DBUS_RESOURCED_FAKE_RESOURCED_CLIENT_H_

#include "base/component_export.h"
#include "chromeos/dbus/resourced/resourced_client.h"

namespace chromeos {

class COMPONENT_EXPORT(RESOURCED) FakeResourcedClient : public ResourcedClient {
 public:
  FakeResourcedClient();
  ~FakeResourcedClient() override;

  FakeResourcedClient(const FakeResourcedClient&) = delete;
  FakeResourcedClient& operator=(const FakeResourcedClient&) = delete;

  // ResourcedClient:
  void GetAvailableMemoryKB(DBusMethodCallback<uint64_t> callback) override;

  // Get memory margins.
  void GetMemoryMarginsKB(
      DBusMethodCallback<ResourcedClient::MemoryMarginsKB> callback) override;

  void SetGameMode(bool state, DBusMethodCallback<bool> callback) override;

  void set_set_game_mode_response(absl::optional<bool> set_game_mode_response) {
    set_game_mode_response_ = set_game_mode_response;
  }

  int get_enter_game_mode_count() const { return enter_game_mode_count_; }

  int get_exit_game_mode_count() const { return exit_game_mode_count_; }

 private:
  absl::optional<bool> set_game_mode_response_;

  int enter_game_mode_count_ = 0;
  int exit_game_mode_count_ = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_RESOURCED_FAKE_RESOURCED_CLIENT_H_
