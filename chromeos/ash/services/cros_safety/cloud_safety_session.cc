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

namespace {

using cros_safety::mojom::SafetyClassifierVerdict;
using cros_safety::mojom::SafetyRuleset;
using ImageType = manta::WalrusProvider::ImageType;

std::optional<ImageType> ToWalrusImageType(SafetyRuleset ruleset) {
  switch (ruleset) {
    case SafetyRuleset::kMantisInputImage:
      return ImageType::kInputImage;
    case SafetyRuleset::kMantisOutputImage:
      return ImageType::kOutputImage;
    case SafetyRuleset::kMantisGeneratedRegion:
      return ImageType::kGeneratedRegion;
    case SafetyRuleset::kMantisGeneratedRegionOutpainting:
      return ImageType::kGeneratedRegionOutpainting;
    default:
      LOG(WARNING) << "Unexpected ruleset for Walrus filter: " << ruleset;
      return std::nullopt;
  }
}

SafetyClassifierVerdict ToSafetyClassifierVerdict(
    manta::MantaStatusCode status_code,
    bool is_classify_image) {
  switch (status_code) {
    case manta::MantaStatusCode::kOk:
      return SafetyClassifierVerdict::kPass;
    case manta::MantaStatusCode::kBlockedOutputs:
      return is_classify_image ? SafetyClassifierVerdict::kFailedImage
                               : SafetyClassifierVerdict::kFailedText;
    case manta::MantaStatusCode::kInvalidInput:
      return SafetyClassifierVerdict::kInvalidInput;
    case manta::MantaStatusCode::kBackendFailure:
      return SafetyClassifierVerdict::kBackendFailure;
    case manta::MantaStatusCode::kNoInternetConnection:
      return SafetyClassifierVerdict::kNoInternetConnection;
    default:
      return SafetyClassifierVerdict::kGenericError;
  }
}

void OnClassifyTextSafetyComplete(
    CloudSafetySession::ClassifySafetyCallback callback,
    base::Value::Dict dict,
    manta::MantaStatus status) {
  SafetyClassifierVerdict ret_val =
      ToSafetyClassifierVerdict(status.status_code, /* is_image*/ false);
  std::move(callback).Run(ret_val);
}

void OnClassifyImageSafetyComplete(
    CloudSafetySession::ClassifySafetyCallback callback,
    base::Value::Dict dict,
    manta::MantaStatus status) {
  SafetyClassifierVerdict ret_val =
      ToSafetyClassifierVerdict(status.status_code, /* is_image*/ true);
  std::move(callback).Run(ret_val);
}

}  // namespace

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

void CloudSafetySession::ClassifyTextSafety(SafetyRuleset ruleset,
                                            const std::string& text,
                                            ClassifySafetyCallback callback) {
  walrus_provider_->Filter(
      text, base::BindOnce(&OnClassifyTextSafetyComplete, std::move(callback)));
}

void CloudSafetySession::ClassifyImageSafety(
    SafetyRuleset ruleset,
    const std::optional<std::string>& text,
    mojo_base::BigBuffer image,
    ClassifySafetyCallback callback) {
  std::vector<std::vector<uint8_t>> images;
  images.push_back(std::vector<uint8_t>(image.begin(), image.end()));
  std::optional<ImageType> filter_type = ToWalrusImageType(ruleset);
  if (filter_type.has_value()) {
    std::vector<ImageType> image_types;
    image_types.push_back(filter_type.value());
    walrus_provider_->Filter(
        text, images, image_types,
        base::BindOnce(&OnClassifyImageSafetyComplete, std::move(callback)));
  } else {
    walrus_provider_->Filter(
        text, images,
        base::BindOnce(&OnClassifyImageSafetyComplete, std::move(callback)));
  }
}

}  // namespace ash
