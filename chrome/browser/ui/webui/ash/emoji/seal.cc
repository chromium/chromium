// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/emoji/seal.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/emoji/seal_utils.h"
#include "components/manta/manta_status.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace ash {

namespace {
constexpr gfx::Size kImageDimension(1024, 1024);

manta::proto::Request CreateSnapperRequest(std::string_view text) {
  manta::proto::Request request;
  // TODO(b/311086550): Replace this with Seal specific feature name.
  request.set_feature_name(manta::proto::FeatureName::CHROMEOS_WALLPAPER);

  manta::proto::RequestConfig& request_config =
      *request.mutable_request_config();
  request_config.set_generation_seed(200);
  request_config.set_num_outputs(20);
  request_config.set_image_resolution(
      manta::proto::ImageResolution::RESOLUTION_1024);

  manta::proto::InputData& input_data = *request.add_input_data();
  input_data.set_text(text.data(), text.size());

  return request;
}

std::vector<seal::mojom::ImagePtr> ParseSnapperResponse(
    std::unique_ptr<manta::proto::Response> response) {
  std::vector<seal::mojom::ImagePtr> ret;
  for (const auto& data : response->output_data()) {
    // Build Data URL.
    const std::string& bytes = data.image().serialized_bytes();
    const std::string data_url =
        base::StrCat({"data:image/png;base64,", base::Base64Encode(bytes)});
    ret.push_back(seal::mojom::Image::New(GURL(data_url), kImageDimension));
  }
  return ret;
}
}  // namespace

SealService::SealService(
    mojo::PendingReceiver<seal::mojom::SealService> receiver,
    std::unique_ptr<manta::SnapperProvider> snapper_provider)
    : receiver_(this, std::move(receiver)),
      snapper_provider_(std::move(snapper_provider)) {}

SealService::~SealService() = default;

void SealService::GetImages(const std::string& query,
                            GetImagesCallback callback) {
  if (!snapper_provider_) {
    std::move(callback).Run(seal::mojom::Status::kNotEnabledError,
                            std::vector<seal::mojom::ImagePtr>{});
    return;
  }
  // TODO(b:330263928): Add real traffic annotation.
  manta::proto::Request request = CreateSnapperRequest(query);
  snapper_provider_->Call(request, MISSING_TRAFFIC_ANNOTATION,
                          base::BindOnce(&SealService::HandleSnapperResponse,
                                         weak_ptr_factory_.GetWeakPtr(), query,
                                         std::move(callback)));
}

void SealService::HandleSnapperResponse(
    const std::string& query,
    GetImagesCallback callback,
    std::unique_ptr<manta::proto::Response> response,
    manta::MantaStatus status) {
  if (status.status_code != manta::MantaStatusCode::kOk) {
    std::move(callback).Run(seal::mojom::Status::kUnknownError,
                            std::vector<seal::mojom::ImagePtr>{});
    return;
  }
  std::move(callback).Run(seal::mojom::Status::kOk,
                          ParseSnapperResponse(std::move(response)));
}

}  // namespace ash
