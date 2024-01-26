// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_SECURE_CHANNEL_STRUCTURED_METRICS_LOGGER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_SECURE_CHANNEL_STRUCTURED_METRICS_LOGGER_H_

namespace ash::secure_channel {

class SecureChannelStructuredMetricsLogger
    : public mojom::SecureChannelStructuredMetricsLogger {
 public:
 public:
  SecureChannelStructuredMetricsLogger();
  ~SecureChannelStructuredMetricsLogger() override;

  mojo::PendingRemote<mojom::SecureChannelStructuredMetricsLogger>
  GenerateRemote();

  void UnbindReceiver();

 private:
  mojo::Receiver<mojom::SecureChannelStructuredMetricsLogger> receiver_{this};
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_SECURE_CHANNEL_STRUCTURED_METRICS_LOGGER_H_
