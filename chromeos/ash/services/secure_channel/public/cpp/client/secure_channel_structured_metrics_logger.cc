// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_structured_metrics_logger.h"

namespace ash::secure_channel {

SecureChannelStructuredMetricsLogger::SecureChannelStructuredMetricsLogger() =
    default;
SecureChannelStructuredMetricsLogger::~SecureChannelStructuredMetricsLogger() =
    default;

mojo::PendingRemote<mojom::SecureChannelStructuredMetricsLogger>
SecureChannelStructuredMetricsLogger::GenerateRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void SecureChannelStructuredMetricsLogger::UnbindReceiver() {
  receiver_.reset();
}
}  // namespace ash::secure_channel
