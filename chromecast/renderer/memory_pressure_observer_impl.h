// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_MEMORY_PRESSURE_OBSERVER_IMPL_H_
#define CHROMECAST_RENDERER_MEMORY_PRESSURE_OBSERVER_IMPL_H_

#include "base/macros.h"
#include "chromecast/common/mojom/memory_pressure.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromecast {

class MemoryPressureObserverImpl : public mojom::MemoryPressureObserver {
 public:
  MemoryPressureObserverImpl(
      mojo::PendingRemote<mojom::MemoryPressureObserver>* observer);
  ~MemoryPressureObserverImpl() override;

 private:
  void MemoryPressureLevelChanged(int32_t pressure_level) override;

  mojo::Receiver<mojom::MemoryPressureObserver> receiver_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPressureObserverImpl);
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_MEMORY_PRESSURE_OBSERVER_IMPL_H_
