// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_LAYERED_WINDOW_UPDATER_IMPL_H_
#define COMPONENTS_VIZ_HOST_LAYERED_WINDOW_UPDATER_IMPL_H_

#include <windows.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "components/viz/host/viz_host_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/compositing/layered_window_updater.mojom.h"
#include "ui/gfx/geometry/size.h"

class SkCanvas;

namespace viz {

// Makes layered window drawing syscalls. Updates a layered window from shared
// memory backing buffer that was drawn into by the GPU process. This is
// required as UpdateLayeredWindow() syscall is blocked by the GPU sandbox.
class VIZ_HOST_EXPORT LayeredWindowUpdaterImpl
    : public mojom::LayeredWindowUpdater {
 public:
  LayeredWindowUpdaterImpl(
      HWND hwnd,
      mojo::PendingReceiver<mojom::LayeredWindowUpdater> receiver);
  ~LayeredWindowUpdaterImpl() override;

  // mojom::LayeredWindowUpdater implementation.
  void OnAllocatedSharedMemory(const gfx::Size& pixel_size,
                               base::UnsafeSharedMemoryRegion region) override;
  void Draw(DrawCallback draw_callback) override;

 private:
  const HWND hwnd_;
  mojo::Receiver<mojom::LayeredWindowUpdater> receiver_;
  std::unique_ptr<SkCanvas> canvas_;

  DISALLOW_COPY_AND_ASSIGN(LayeredWindowUpdaterImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_LAYERED_WINDOW_UPDATER_IMPL_H_
