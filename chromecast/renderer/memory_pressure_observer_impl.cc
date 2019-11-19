// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/memory_pressure_observer_impl.h"

#include "base/memory/memory_pressure_listener.h"

namespace chromecast {

MemoryPressureObserverImpl::MemoryPressureObserverImpl(
    mojo::PendingRemote<mojom::MemoryPressureObserver>* observer)
    : receiver_(this, observer->InitWithNewPipeAndPassReceiver()) {}

MemoryPressureObserverImpl::~MemoryPressureObserverImpl() = default;

void MemoryPressureObserverImpl::MemoryPressureLevelChanged(
    int32_t pressure_level) {
  base::MemoryPressureListener::NotifyMemoryPressure(
      static_cast<base::MemoryPressureListener::MemoryPressureLevel>(
          pressure_level));
}

}  // namespace chromecast
