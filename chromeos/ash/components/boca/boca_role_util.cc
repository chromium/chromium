// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_role_util.h"

#include "ash/constants/ash_features.h"

namespace ash::boca_util {
bool IsProducer() {
  return features::IsBocaEnabled() && !features::IsBocaConsumerEnabled();
}

bool IsConsumer() {
  return features::IsBocaEnabled() && features::IsBocaConsumerEnabled();
}

bool IsEnabled() {
  return features::IsBocaEnabled();
}
}  // namespace ash::boca_util
