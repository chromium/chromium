// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"

#include "base/callback.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"

namespace chromeos {

namespace {

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

mojo::Remote<cros_healthd::mojom::CrosHealthdService>
FakeCrosHealthdClient::BootstrapMojoConnection(
    base::OnceCallback<void(bool success)> result_callback) {
  mojo::Remote<cros_healthd::mojom::CrosHealthdService> remote(
      receiver_.BindNewPipeAndPassRemote());

  std::move(result_callback).Run(/*success=*/true);
  return remote;
}

void FakeCrosHealthdClient::SetProbeTelemetryInfoResponseForTesting(
    TelemetryInfoPtr& info) {
  fake_service_.SetProbeTelemetryInfoResponseForTesting(info);
}

}  // namespace chromeos
