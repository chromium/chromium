// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_HISTOGRAM_CHILD_PROCESS_H_
#define COMPONENTS_METRICS_HISTOGRAM_CHILD_PROCESS_H_

#include "components/metrics/public/mojom/histogram_fetcher.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace metrics {

// Interface that is implemented by the various child process types that can
// be added to HistogramController for collecting histogram data.
class HistogramChildProcess {
 public:
  // Called to connect to a ChildHistogramFetcherFactory implementation in the
  // child process.
  virtual void BindChildHistogramFetcherFactory(
      mojo::PendingReceiver<mojom::ChildHistogramFetcherFactory> factory) = 0;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_HISTOGRAM_CHILD_PROCESS_H_
