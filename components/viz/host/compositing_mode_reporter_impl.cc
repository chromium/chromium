// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/compositing_mode_reporter_impl.h"

namespace viz {

CompositingModeReporterImpl::CompositingModeReporterImpl() = default;

CompositingModeReporterImpl::~CompositingModeReporterImpl() = default;

void CompositingModeReporterImpl::BindReceiver(
    mojo::PendingReceiver<mojom::CompositingModeReporter> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void CompositingModeReporterImpl::SetUsingSoftwareCompositing() {
  gpu_ = false;
  for (auto& it : watchers_)
    it->CompositingModeFallbackToSoftware();
}

void CompositingModeReporterImpl::AddCompositingModeWatcher(
    mojo::PendingRemote<mojom::CompositingModeWatcher> watcher) {
  mojo::Remote<mojom::CompositingModeWatcher> watcher_remote(
      std::move(watcher));
  if (!gpu_)
    watcher_remote->CompositingModeFallbackToSoftware();

  watchers_.Add(std::move(watcher_remote));
}

}  // namespace viz
