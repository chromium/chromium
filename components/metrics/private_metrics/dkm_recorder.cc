// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/dkm_recorder.h"

#include "base/no_destructor.h"

namespace metrics::private_metrics {

DkmRecorder::DkmRecorder() = default;

DkmRecorder::~DkmRecorder() = default;

// static
DkmRecorder* DkmRecorder::Get() {
  static base::NoDestructor<DkmRecorder> recorder;
  return recorder.get();
}

void DkmRecorder::AddEntry(mojom::PrivateMetricsEntryPtr entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  entries_.push_back(std::move(entry));
}

}  // namespace metrics::private_metrics
