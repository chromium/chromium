// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_COMPOSITING_MODE_REPORTER_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_COMPOSITING_MODE_REPORTER_IMPL_H_

#include "base/macros.h"
#include "components/viz/service/viz_service_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/viz/public/mojom/compositing/compositing_mode_watcher.mojom.h"

namespace viz {

class VIZ_SERVICE_EXPORT CompositingModeReporterImpl
    : public mojom::CompositingModeReporter {
 public:
  // Creates the CompositingModeReporterImpl and binds it to the deferred mojo
  // pointer behind the |request|.
  CompositingModeReporterImpl();
  ~CompositingModeReporterImpl() override;

  // Called for each consumer of the CompositingModeReporter interface, to
  // bind a receiver for them.
  void BindReceiver(
      mojo::PendingReceiver<mojom::CompositingModeReporter> receiver);

  // Call to inform the reporter that software compositing is being used instead
  // of gpu. This is a one-way setting that can not be reverted. This will
  // notify any registered CompositingModeWatchers.
  void SetUsingSoftwareCompositing();

  // mojom::CompositingModeReporter implementation.
  void AddCompositingModeWatcher(
      mojo::PendingRemote<mojom::CompositingModeWatcher> watcher) override;

 private:
  bool gpu_ = true;
  mojo::ReceiverSet<mojom::CompositingModeReporter> receivers_;
  mojo::RemoteSet<mojom::CompositingModeWatcher> watchers_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_COMPOSITING_MODE_REPORTER_IMPL_H_
