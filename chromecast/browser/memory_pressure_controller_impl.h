// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_MEMORY_PRESSURE_CONTROLLER_IMPL_H_
#define CHROMECAST_BROWSER_MEMORY_PRESSURE_CONTROLLER_IMPL_H_

#include "base/memory/memory_pressure_listener.h"
#include "chromecast/common/mojom/memory_pressure.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromecast {

class MemoryPressureControllerImpl : public mojom::MemoryPressureController,
                                     public base::MemoryPressureListener {
 public:
  MemoryPressureControllerImpl();

  MemoryPressureControllerImpl(const MemoryPressureControllerImpl&) = delete;
  MemoryPressureControllerImpl& operator=(const MemoryPressureControllerImpl&) =
      delete;

  ~MemoryPressureControllerImpl() override;

  void AddReceiver(
      mojo::PendingReceiver<mojom::MemoryPressureController> receiver);

 private:
  // chromecast::mojom::MemoryPressure implementation.
  void AddObserver(
      mojo::PendingRemote<mojom::MemoryPressureObserver> observer) override;

  // base::MemoryPressureListener:
  void OnMemoryPressure(base::MemoryPressureLevel level) override;

  mojo::RemoteSet<mojom::MemoryPressureObserver> observers_;
  mojo::ReceiverSet<mojom::MemoryPressureController> receivers_;

  base::MemoryPressureListenerRegistration
      memory_pressure_listener_registration_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_MEMORY_PRESSURE_CONTROLLER_IMPL_H_
