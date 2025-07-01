// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/logging/ml_log_router.h"

#include "components/autofill/core/browser/ml_model/logging/autofill_ml_internals.mojom.h"

namespace autofill {

MLLogRouter::MLLogRouter() = default;

MLLogRouter::~MLLogRouter() = default;

void MLLogRouter::ProcessLog(
    autofill_ml_internals::mojom::MLPredictionLogPtr log) {
  for (MLLogReceiver& receiver : receivers_) {
    receiver.ProcessLog(*log);
  }
}

bool MLLogRouter::HasReceivers() const {
  return !receivers_.empty();
}

void MLLogRouter::AddObserver(MLLogReceiver* receiver) {
  receivers_.AddObserver(receiver);
}

void MLLogRouter::RemoveObserver(MLLogReceiver* receiver) {
  receivers_.RemoveObserver(receiver);
}

}  // namespace autofill
