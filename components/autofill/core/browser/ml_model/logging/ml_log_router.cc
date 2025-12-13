// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/logging/ml_log_router.h"

#include "components/autofill/core/browser/ml_model/logging/autofill_ml_internals.mojom.h"

namespace autofill {

MlLogRouter::MlLogRouter() = default;

MlLogRouter::~MlLogRouter() = default;

void MlLogRouter::ProcessLog(
    autofill_ml_internals::mojom::MlPredictionLogPtr log) {
  for (MlLogReceiver& receiver : receivers_) {
    receiver.ProcessLog(*log);
  }
}

bool MlLogRouter::HasReceivers() const {
  return !receivers_.empty();
}

void MlLogRouter::AddObserver(MlLogReceiver* receiver) {
  receivers_.AddObserver(receiver);
}

void MlLogRouter::RemoveObserver(MlLogReceiver* receiver) {
  receivers_.RemoveObserver(receiver);
}

}  // namespace autofill
