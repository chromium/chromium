// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/lobster/image_fetcher.h"

#include <string_view>

#include "base/logging.h"
#include "components/manta/proto/manta.pb.h"
#include "components/manta/snapper_provider.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/geometry/size.h"

namespace {

constexpr gfx::Size kPreviewImageSize = gfx::Size(512, 512);

}  // namespace

ImageFetcher::ImageFetcher(manta::SnapperProvider* provider)
    : provider_(provider) {}

ImageFetcher::~ImageFetcher() = default;

void ImageFetcher::RequestPreviewCandidates(
    std::string_view query,
    int num_candidates,
    ash::RequestCandidatesCallback callback) {
  if (provider_ == nullptr) {
    LOG(ERROR) << "Provider is not available";
    std::move(callback).Run({});
    return;
  }

  manta::proto::Request request;
  manta::proto::RequestConfig& request_config =
      *request.mutable_request_config();
  manta::proto::ImageDimensions& image_dimensions =
      *request_config.mutable_image_dimensions();

  manta::proto::InputData& input_data = *request.add_input_data();
  request_config.set_num_outputs(num_candidates);
  request.set_feature_name(manta::proto::FeatureName::CHROMEOS_LOBSTER);
  image_dimensions.set_width(kPreviewImageSize.width());
  image_dimensions.set_height(kPreviewImageSize.height());
  input_data.set_text(query.data(), query.size());

  // TODO(b:354620949): MISSING_TRAFFIC_ANNOTATION should be resolved before
  // launch.
  provider_->Call(
      request, MISSING_TRAFFIC_ANNOTATION,
      base::BindOnce(&ImageFetcher::OnRequestPreviewCandidates,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ImageFetcher::OnRequestPreviewCandidates(
    ash::RequestCandidatesCallback callback,
    std::unique_ptr<manta::proto::Response> response,
    manta::MantaStatus status) {
  // TODO(b:348282335): Add image sanitization.
  std::move(callback).Run({});
}
