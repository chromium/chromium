// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STARTUP_METRIC_UTILS_BROWSER_STARTUP_METRIC_HOST_IMPL_H_
#define COMPONENTS_STARTUP_METRIC_UTILS_BROWSER_STARTUP_METRIC_HOST_IMPL_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/startup_metric_utils/common/startup_metric.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace startup_metric_utils {

class StartupMetricHostImpl : public mojom::StartupMetricHost {
 public:
  StartupMetricHostImpl();
  ~StartupMetricHostImpl() override;

  static void Create(mojo::PendingReceiver<mojom::StartupMetricHost> receiver);

 private:
  void RecordRendererMainEntryTime(
      base::TimeTicks renderer_main_entry_time) override;

  DISALLOW_COPY_AND_ASSIGN(StartupMetricHostImpl);
};

}  // namespace startup_metric_utils

#endif  // COMPONENTS_STARTUP_METRIC_UTILS_BROWSER_STARTUP_METRIC_HOST_IMPL_H_
