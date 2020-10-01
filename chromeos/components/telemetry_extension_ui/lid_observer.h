// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_LID_OBSERVER_H_
#define CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_LID_OBSERVER_H_

#if defined(OFFICIAL_BUILD)
#error Lid observer should only be included in unofficial builds.
#endif

#include "chromeos/components/telemetry_extension_ui/mojom/system_events_service.mojom-forward.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {

class LidObserver : public cros_healthd::mojom::CrosHealthdLidObserver {
 public:
  LidObserver();
  LidObserver(const LidObserver&) = delete;
  LidObserver& operator=(const LidObserver&) = delete;
  ~LidObserver() override;

  void AddObserver(mojo::PendingRemote<health::mojom::LidObserver> observer);

  void OnLidClosed() override;
  void OnLidOpened() override;

  // Waits until disconnect handler will be triggered if fake cros_healthd was
  // shutdown.
  void FlushForTesting();

 private:
  void Connect();

  mojo::Receiver<cros_healthd::mojom::CrosHealthdLidObserver> receiver_;
  mojo::RemoteSet<health::mojom::LidObserver> observers_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_LID_OBSERVER_H_
