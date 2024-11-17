// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/walrus_provider.h"

#include <algorithm>

#include "components/manta/features.h"
#include "third_party/skia/include/codec/SkJpegDecoder.h"
#include "third_party/skia/include/codec/SkPngDecoder.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace manta {

namespace {

constexpr char kOauthConsumerName[] = "manta_walrus";
constexpr base::TimeDelta kTimeout = base::Seconds(30);
// The maximum number of pixels after resizing an image.
constexpr int32_t kMaxPixelsAfterResizing = 1024 * 1024;

void OnServerResponseOrErrorReceived(
    MantaGenericCallback callback,
    std::unique_ptr<proto::Response> manta_response,
    MantaStatus manta_status) {
  if (manta_response == nullptr || !manta_response->filtered_data_size()) {
    // Return the status if the text/images are not blocked.
    std::move(callback).Run(base::Value::Dict(), std::move(manta_status));
    return;
  }

  CHECK(manta_response != nullptr);

  // Add extra information for the invalid inputs.
  auto output_data = base::Value::Dict();
  for (const auto& filtered_data : manta_response->filtered_data()) {
    auto filtered_reason = filtered_data.reason();
    switch (filtered_reason) {
      case manta::proto::FilteredReason::IMAGE_SAFETY:
        output_data.Set("image_blocked", true);
        break;
      case manta::proto::FilteredReason::TEXT_SAFETY:
        output_data.Set("text_blocked", true);
        break;
      default:
        break;
    }
  }

  std::move(callback).Run(std::move(output_data),
                          {manta::MantaStatusCode::kBlockedOutputs});
}

}  // namespace

WalrusProvider::WalrusProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    const ProviderParams& provider_params)
    : BaseProvider(url_loader_factory, identity_manager, provider_params) {}

WalrusProvider::WalrusProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : BaseProvider(url_loader_factory, identity_manager) {}

WalrusProvider::~WalrusProvider() = default;

void WalrusProvider::Filter(std::string text_prompt,
                            MantaGenericCallback done_callback) {
  std::vector<std::vector<uint8_t>> empty_images;
  Filter(text_prompt, empty_images, std::move(done_callback));
}

std::optional<SkBitmap> DeserializeImage(const std::vector<uint8_t>& bytes) {
  if (SkJpegDecoder::IsJpeg(bytes.data(), bytes.size())) {
    return gfx::JPEGCodec::Decode(bytes);
  }
  if (SkPngDecoder::IsPng(bytes.data(), bytes.size())) {
    return gfx::PNGCodec::Decode(bytes);
  }
  return std::nullopt;
}

// Downscales the given image maintaining the aspect ratio if the number of
// pixels exceeds |max_pixels_after_resizing|. If the image cannot be decoded or
// encoded, returns null.
std::optional<std::vector<uint8_t>> WalrusProvider::DownscaleImageIfNeeded(
    const std::vector<uint8_t>& image_bytes,
    int32_t max_pixels_after_resizing = kMaxPixelsAfterResizing) {
  auto bitmap = DeserializeImage(image_bytes);
  if (!bitmap.has_value() || bitmap->height() == 0 || bitmap->width() == 0) {
    return std::nullopt;
  }

  // (height * scale) * (width * scale) = max_pixels_after_resizing
  // scale = sqrt(max_pixels_after_resizing / (height * width))
  double scale = std::sqrt(static_cast<double>(max_pixels_after_resizing) /
                           (bitmap->height() * bitmap->width()));
  scale = std::min(1.0, scale);

  SkBitmap resized_bitmap = skia::ImageOperations::Resize(
      bitmap.value(), skia::ImageOperations::RESIZE_BEST,
      scale * bitmap->width(), scale * bitmap->height());

  return gfx::JPEGCodec::Encode(resized_bitmap, /*quality=*/90);
}

void WalrusProvider::Filter(const std::optional<std::string>& text_prompt,
                            const std::vector<std::vector<uint8_t>>& images,
                            MantaGenericCallback done_callback) {
  proto::Request request;
  request.set_feature_name(proto::FeatureName::CHROMEOS_WALRUS);

  if (text_prompt.has_value() && !text_prompt->empty()) {
    auto* input_data = request.add_input_data();
    input_data->set_tag("input_text");
    input_data->set_text(text_prompt.value());
  }

  for (auto& image : images) {
    auto* input_data = request.add_input_data();
    input_data->set_tag("input_image");
    auto resized_image_bytes = DownscaleImageIfNeeded(image);
    if (resized_image_bytes.has_value()) {
      input_data->mutable_image()->set_serialized_bytes(
          std::string(resized_image_bytes.value().begin(),
                      resized_image_bytes.value().end()));
    } else {
      input_data->mutable_image()->set_serialized_bytes(
          std::string(image.begin(), image.end()));
    }
  }

  if (!request.input_data_size()) {
    std::move(done_callback)
        .Run(base::Value::Dict(), {MantaStatusCode::kInvalidInput});
    return;
  }

  // TODO(b:370476808): MISSING_TRAFFIC_ANNOTATION should be resolved before
  // launch.
  RequestInternal(
      GURL{GetProviderEndpoint(features::IsWalrusUseProdServerEnabled())},
      kOauthConsumerName, MISSING_TRAFFIC_ANNOTATION, request,
      MantaMetricType::kWalrus,
      base::BindOnce(&OnServerResponseOrErrorReceived,
                     std::move(done_callback)),
      kTimeout);
}

}  // namespace manta
