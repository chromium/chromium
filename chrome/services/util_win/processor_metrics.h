// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_UTIL_WIN_PROCESSOR_METRICS_H_
#define CHROME_SERVICES_UTIL_WIN_PROCESSOR_METRICS_H_

#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

void RecordProcessorMetricsForTesting();

class ProcessorMetricsImpl : public chrome::mojom::ProcessorMetrics {
 public:
  explicit ProcessorMetricsImpl(
      mojo::PendingReceiver<chrome::mojom::ProcessorMetrics> receiver);
  ~ProcessorMetricsImpl() override;
  ProcessorMetricsImpl(const ProcessorMetricsImpl&) = delete;
  ProcessorMetricsImpl& operator=(const ProcessorMetricsImpl&) = delete;

 private:
  // chrome::mojom::ProcessorMetrics:
  void RecordProcessorMetrics(RecordProcessorMetricsCallback callback) override;

  mojo::Receiver<chrome::mojom::ProcessorMetrics> receiver_;
};

#endif  // CHROME_SERVICES_UTIL_WIN_PROCESSOR_METRICS_H_
