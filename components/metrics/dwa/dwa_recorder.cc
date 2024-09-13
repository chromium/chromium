// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_recorder.h"

#include "base/no_destructor.h"

namespace metrics::dwa {

DwaRecorder::DwaRecorder() = default;

DwaRecorder::~DwaRecorder() = default;

DwaRecorder* DwaRecorder::Get() {
  static base::NoDestructor<DwaRecorder> recorder;
  return recorder.get();
}

// static
void DwaRecorder::AddEntry(metrics::dwa::mojom::DwaEntryPtr entry) {
  // TODO(b/359556688): To be implemented, `static` to be re-evaluated.
  return;
}

}  // namespace metrics::dwa
