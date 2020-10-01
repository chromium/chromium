// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_SYSTEM_EVENTS_SERVICE_H_
#define CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_SYSTEM_EVENTS_SERVICE_H_

#if defined(OFFICIAL_BUILD)
#error System events service should only be included in unofficial builds.
#endif

#include "chromeos/components/telemetry_extension_ui/lid_observer.h"
#include "chromeos/components/telemetry_extension_ui/mojom/system_events_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

class SystemEventsService : public health::mojom::SystemEventsService {
 public:
  explicit SystemEventsService(
      mojo::PendingReceiver<health::mojom::SystemEventsService> receiver);
  SystemEventsService(const SystemEventsService&) = delete;
  SystemEventsService& operator=(const SystemEventsService&) = delete;
  ~SystemEventsService() override;

  void AddLidObserver(
      mojo::PendingRemote<health::mojom::LidObserver> observer) override;

  void FlushForTesting();

 private:
  mojo::Receiver<health::mojom::SystemEventsService> receiver_;

  LidObserver lid_observer_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_SYSTEM_EVENTS_SERVICE_H_
