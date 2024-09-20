// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_recorder_interface.h"

#include "components/metrics/dwa/dwa_recorder.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace metrics::dwa {

DwaRecorderInterface::DwaRecorderInterface(DwaRecorder* dwa_recorder)
    : dwa_recorder_(dwa_recorder) {}

DwaRecorderInterface::~DwaRecorderInterface() = default;

// static
void DwaRecorderInterface::Create(
    DwaRecorder* dwa_recorder,
    mojo::PendingReceiver<metrics::dwa::mojom::DwaRecorderInterface> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<DwaRecorderInterface>(dwa_recorder),
      std::move(receiver));
}

void DwaRecorderInterface::AddEntry(
    metrics::dwa::mojom::DwaEntryPtr dwa_entry) {
  // TODO(b/359556688): To be implemented, `static` to be re-evaluated.
}

}  // namespace metrics::dwa
