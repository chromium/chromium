// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/cros_healthd/fake_cros_healthd_client.h"

#include "base/callback.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"

namespace ash::cros_healthd {

namespace {

// TODO(https://crbug.com/1164001): remove after migration to namespace ash.
namespace mojom = ::chromeos::cros_healthd::mojom;

// Used to track the fake instance, mirrors the instance in the base class.
FakeCrosHealthdClient* g_instance = nullptr;

}  // namespace

FakeCrosHealthdClient::FakeCrosHealthdClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakeCrosHealthdClient::~FakeCrosHealthdClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakeCrosHealthdClient* FakeCrosHealthdClient::Get() {
  return g_instance;
}

mojo::Remote<mojom::CrosHealthdServiceFactory>
FakeCrosHealthdClient::BootstrapMojoConnection(
    BootstrapMojoConnectionCallback result_callback) {
  CHECK(bootstrap_callback_) << "Fake Healthd mojo service is not initialized.";
  std::move(result_callback).Run(/*success=*/true);
  return bootstrap_callback_.Run();
}

}  // namespace ash::cros_healthd
