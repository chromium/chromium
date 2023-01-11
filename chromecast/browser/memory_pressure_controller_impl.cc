// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/memory_pressure_controller_impl.h"

#include "base/functional/bind.h"

namespace chromecast {

MemoryPressureControllerImpl::MemoryPressureControllerImpl() {
  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE,
      base::BindRepeating(&MemoryPressureControllerImpl::OnMemoryPressure,
                          base::Unretained(this)));
}

MemoryPressureControllerImpl::~MemoryPressureControllerImpl() = default;

void MemoryPressureControllerImpl::AddReceiver(
    mojo::PendingReceiver<mojom::MemoryPressureController> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MemoryPressureControllerImpl::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  for (auto& observer : observers_)
    observer->MemoryPressureLevelChanged(level);
}

void MemoryPressureControllerImpl::AddObserver(
    mojo::PendingRemote<mojom::MemoryPressureObserver> observer) {
  observers_.Add(std::move(observer));
}

}  // namespace chromecast
