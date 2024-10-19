// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_safety/cloud_safety_session.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/cros_safety/public/mojom/cros_safety.mojom.h"
#include "components/manta/features.h"
#include "components/manta/manta_service.h"

namespace ash {

void OnRecieveClassifySafetyResults(
    CloudSafetySession::ClassifySafetyCallback callback,
    base::Value::Dict dict,
    manta::MantaStatus status) {
  switch (status.status_code) {
    case manta::MantaStatusCode::kOk:
      std::move(callback).Run(
          cros_safety::mojom::SafetyClassifierVerdict::kPass);
      break;
    default:
      std::move(callback).Run(
          cros_safety::mojom::SafetyClassifierVerdict::kGenericError);
  }
}

CloudSafetySession::CloudSafetySession(
    std::unique_ptr<manta::WalrusProvider> walrus_provider)
    : walrus_provider_(std::move(walrus_provider)) {
  CHECK(walrus_provider_);
}

CloudSafetySession::~CloudSafetySession() = default;

void CloudSafetySession::AddReceiver(
    mojo::PendingReceiver<cros_safety::mojom::CloudSafetySession> receiver) {
  receiver_set_.Add(this, std::move(receiver),
                    base::SequencedTaskRunner::GetCurrentDefault());
}

void CloudSafetySession::ClassifyTextSafety(
    cros_safety::mojom::SafetyRuleset ruleset,
    const std::string& text,
    ClassifySafetyCallback callback) {
  walrus_provider_->Filter(text, base::BindOnce(&OnRecieveClassifySafetyResults,
                                                std::move(callback)));
}
void CloudSafetySession::ClassifyImageSafety(
    cros_safety::mojom::SafetyRuleset ruleset,
    const std::optional<std::string>& text,
    mojo_base::BigBuffer image,
    ClassifySafetyCallback callback) {
  std::vector<std::vector<uint8_t>> images;
  images.push_back(std::vector<uint8_t>(image.begin(), image.end()));
  walrus_provider_->Filter(
      text, images,
      base::BindOnce(&OnRecieveClassifySafetyResults, std::move(callback)));
}

}  // namespace ash
