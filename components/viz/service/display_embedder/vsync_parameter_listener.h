// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_VSYNC_PARAMETER_LISTENER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_VSYNC_PARAMETER_LISTENER_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "components/viz/service/viz_service_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/vsync_parameter_observer.mojom.h"

namespace viz {

// Sends updated vsync parameters over IPC when either the interval changes or
// the timebase offset skews more than kMaxTimebaseSkew. Timebase skew for last
// timebase sent to the observer T1 and a new timebase T2 is defined as
// difference between T1 + N * interval and T2 + M * interval for N and M that
// produce the smallest difference.
class VIZ_SERVICE_EXPORT VSyncParameterListener {
 public:
  explicit VSyncParameterListener(
      mojo::PendingRemote<mojom::VSyncParameterObserver> observer);
  ~VSyncParameterListener();

  void OnVSyncParametersUpdated(base::TimeTicks timebase,
                                base::TimeDelta interval);

 private:
  friend class VSyncParameterListenerTestRunner;

  static constexpr base::TimeDelta kMaxTimebaseSkew =
      base::TimeDelta::FromMicroseconds(25);

  bool ShouldSendUpdate(base::TimeTicks timebase, base::TimeDelta interval);

  mojo::Remote<mojom::VSyncParameterObserver> observer_;

  base::TimeDelta last_interval_;
  base::TimeDelta last_offset_;

  DISALLOW_COPY_AND_ASSIGN(VSyncParameterListener);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_VSYNC_PARAMETER_LISTENER_H_
