// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/audio/fake_floss_media_client.h"

#include <utility>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"

namespace ash {

namespace {

FakeFlossMediaClient* g_instance = nullptr;

}  // namespace

FakeFlossMediaClient::FakeFlossMediaClient() {
  CHECK(!g_instance);
  g_instance = this;

  VLOG(1) << "FakeFlossMediaClient is created";
}

FakeFlossMediaClient::~FakeFlossMediaClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakeFlossMediaClient* FakeFlossMediaClient::Get() {
  return g_instance;
}

void FakeFlossMediaClient::SetPlayerPlaybackStatus(
    const std::string& playback_status) {
  NOTIMPLEMENTED();
}

void FakeFlossMediaClient::SetPlayerIdentity(
    const std::string& playback_identity) {
  NOTIMPLEMENTED();
}

void FakeFlossMediaClient::SetPlayerPosition(const int64_t& position) {
  NOTIMPLEMENTED();
}

void FakeFlossMediaClient::SetPlayerDuration(const int64_t& duration) {
  NOTIMPLEMENTED();
}

void FakeFlossMediaClient::SetPlayerMetadata(
    const std::map<std::string, std::string>& metadata) {
  NOTIMPLEMENTED();
}

}  // namespace ash
