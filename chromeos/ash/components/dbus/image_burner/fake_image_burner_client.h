// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_IMAGE_BURNER_FAKE_IMAGE_BURNER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_IMAGE_BURNER_FAKE_IMAGE_BURNER_CLIENT_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/image_burner/image_burner_client.h"

namespace ash {

// A fake implemetation of ImageBurnerClient. This class does nothing.
class COMPONENT_EXPORT(ASH_DBUS_IMAGE_BURNER) FakeImageBurnerClient
    : public ImageBurnerClient {
 public:
  FakeImageBurnerClient();

  FakeImageBurnerClient(const FakeImageBurnerClient&) = delete;
  FakeImageBurnerClient& operator=(const FakeImageBurnerClient&) = delete;

  ~FakeImageBurnerClient() override;

  // ImageBurnerClient overrides
  void Init(dbus::Bus* bus) override;
  void BurnImage(const std::string& from_path,
                 const std::string& to_path,
                 ErrorCallback error_callback) override;
  void SetEventHandlers(
      BurnFinishedHandler burn_finished_handler,
      const BurnProgressUpdateHandler& burn_progress_update_handler) override;
  void ResetEventHandlers() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_IMAGE_BURNER_FAKE_IMAGE_BURNER_CLIENT_H_
