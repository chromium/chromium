// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/image_burner/fake_image_burner_client.h"

#include <utility>

namespace ash {

FakeImageBurnerClient::FakeImageBurnerClient() = default;

FakeImageBurnerClient::~FakeImageBurnerClient() = default;

void FakeImageBurnerClient::Init(dbus::Bus* bus) {}

void FakeImageBurnerClient::ResetEventHandlers() {}

void FakeImageBurnerClient::BurnImage(const std::string& from_path,
                                      const std::string& to_path,
                                      ErrorCallback error_callback) {}

void FakeImageBurnerClient::SetEventHandlers(
    BurnFinishedHandler burn_finished_handler,
    const BurnProgressUpdateHandler& burn_progress_update_handler) {}

}  // namespace ash
