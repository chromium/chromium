// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/wallpaper_search/wallpaper_search_handler.h"

#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/token.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/ui/webui/cr_components/theme_color_picker/customize_chrome_colors.h"
#include "chrome/common/webui_url_constants.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/wallpaper_search.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/search/ntp_features.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"

namespace {
const char kDescriptorsBaseUrl[] =
    "https://static.corp.google.com/chrome-wallpaper-search/";
// Calculate new dimensions given the width and height that will make the
// smaller dimension equal to goal_size but keep the current aspect ratio.
// The first value in the pair is the width and the second is the height.
std::pair<int, int> CalculateResizeDimensions(int width,
                                              int height,
                                              int goal_size) {
  // Set both dimensions to the goal_size since at least one of them will be
  // that size.
  std::pair<int, int> dimensions(goal_size, goal_size);

  // Find the ratio of the width to the height and do some basic proportion
  // math to create the same ratio with the goal_size.
  // If the ratio is 1, we don't do anything.
  auto aspect_ratio = static_cast<float>(width) / height;
  if (aspect_ratio > 1) {
    dimensions.first = static_cast<int>(goal_size * aspect_ratio);
  } else if (aspect_ratio < 1) {
    dimensions.second = static_cast<int>(goal_size / aspect_ratio);
  }
  return dimensions;
}
}  // namespace

WallpaperSearchHandler::WallpaperSearchHandler(
    mojo::PendingReceiver<
        side_panel::customize_chrome::mojom::WallpaperSearchHandler>
        pending_handler,
    Profile* profile,
    image_fetcher::ImageDecoder* image_decoder)
    : profile_(profile),
      data_decoder_(std::make_unique<data_decoder::DataDecoder>()),
      image_decoder_(*image_decoder),
      receiver_(this, std::move(pending_handler)) {}

WallpaperSearchHandler::~WallpaperSearchHandler() {}

void WallpaperSearchHandler::GetDescriptors(GetDescriptorsCallback callback) {
  callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), nullptr);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("customize_chrome_page_handler", R"(
        semantics {
          sender: "Customize Chrome"
          description:
            "This service downloads different configurations "
            "for Customize Chrome."
          trigger:
            "Opening Customize Chrome on the Desktop NTP, "
            "if Google is the default search provider "
            "and the user is signed in."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "chrome-desktop-ntp@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2023-10-10"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature by signing out or "
            "selecting a non-Google default search engine in Chrome "
            "settings under 'Search Engine'."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url =
      GURL(base::StrCat({kDescriptorsBaseUrl, "descriptors_en-US.json"}));
  resource_request->request_initiator =
      url::Origin::Create(GURL(chrome::kChromeUINewTabURL));
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->DownloadToString(
      profile_->GetURLLoaderFactory().get(),
      base::BindOnce(&WallpaperSearchHandler::OnDescriptorsRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      1024 * 1024);
}

void WallpaperSearchHandler::GetWallpaperSearchResults(
    const std::string& descriptor_a,
    const absl::optional<std::string>& descriptor_b,
    const absl::optional<std::string>& descriptor_c,
    side_panel::customize_chrome::mojom::DescriptorDValuePtr descriptor_d_value,
    GetWallpaperSearchResultsCallback callback) {
  callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback),
      std::vector<
          side_panel::customize_chrome::mojom::WallpaperSearchResultPtr>());
  if (!base::FeatureList::IsEnabled(
          ntp_features::kCustomizeChromeWallpaperSearch) ||
      !base::FeatureList::IsEnabled(
          optimization_guide::features::kOptimizationGuideModelExecution)) {
    return;
  }
  auto* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);
  if (!optimization_guide_keyed_service) {
    return;
  }
  optimization_guide::proto::WallpaperSearchRequest request;
  auto& descriptors = *request.mutable_descriptors();
  descriptors.set_descriptor_a(descriptor_a);
  if (descriptor_b.has_value()) {
    descriptors.set_descriptor_b(*descriptor_b);
  }
  if (descriptor_c.has_value()) {
    descriptors.set_descriptor_c(*descriptor_c);
  }
  if (descriptor_d_value) {
    if (descriptor_d_value->is_color()) {
      descriptors.set_descriptor_d(
          skia::SkColorToHexString(descriptor_d_value->get_color()));
    } else if (descriptor_d_value->is_hue()) {
      descriptors.set_descriptor_d(skia::SkColorToHexString(
          HueToSkColor(descriptor_d_value->get_hue())));
    }
  }
  optimization_guide_keyed_service->ExecuteModel(
      optimization_guide::proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH,
      request,
      base::BindOnce(&WallpaperSearchHandler::OnWallpaperSearchResultsRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WallpaperSearchHandler::SetBackgroundToWallpaperSearchResult(
    const base::Token& result_id) {
  CHECK(base::Contains(wallpaper_search_results_, result_id));
  auto* ntp_custom_background_service =
      NtpCustomBackgroundServiceFactory::GetForProfile(profile_);
  CHECK(ntp_custom_background_service);
  ntp_custom_background_service->SelectLocalBackgroundImage(
      result_id, wallpaper_search_results_[result_id]);
}

void WallpaperSearchHandler::OnDescriptorsRetrieved(
    GetDescriptorsCallback callback,
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    // Network errors (i.e. the server did not provide a response).
    DVLOG(1) << "Request failed with error: " << simple_url_loader_->NetError();
    std::move(callback).Run(nullptr);
    return;
  }

  std::string response;
  response.swap(*response_body);
  // The response may start with . Ignore this.
  const char kXSSIResponsePreamble[] = ")]}'";
  if (base::StartsWith(response, kXSSIResponsePreamble,
                       base::CompareCase::SENSITIVE)) {
    response = response.substr(strlen(kXSSIResponsePreamble));
  }
  data_decoder_->ParseJson(
      response,
      base::BindOnce(&WallpaperSearchHandler::OnDescriptorsJsonParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WallpaperSearchHandler::OnDescriptorsJsonParsed(
    GetDescriptorsCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value() || !result->is_dict()) {
    DVLOG(1) << "Parsing JSON failed: " << result.error();
    std::move(callback).Run(nullptr);
    return;
  }

  const base::Value::List* descriptor_a =
      result->GetDict().FindList("descriptor_a");
  const base::Value::List* descriptor_b =
      result->GetDict().FindList("descriptor_b");
  const base::Value::List* descriptor_c_labels =
      result->GetDict().FindList("descriptor_c");
  if (!descriptor_a || !descriptor_b || !descriptor_c_labels) {
    DVLOG(1) << "Parsing JSON failed: no valid descriptors.";
    std::move(callback).Run(nullptr);
    return;
  }

  std::vector<side_panel::customize_chrome::mojom::DescriptorAPtr>
      mojo_descriptor_a_list;
  if (descriptor_a) {
    for (const auto& descriptor : *descriptor_a) {
      const base::Value::Dict& descriptor_a_dict = descriptor.GetDict();
      auto* category = descriptor_a_dict.FindString("category");
      auto* label_values = descriptor_a_dict.FindList("labels");
      if (!category || !label_values) {
        continue;
      }
      auto mojo_descriptor_a =
          side_panel::customize_chrome::mojom::DescriptorA::New();
      mojo_descriptor_a->category = *category;
      std::vector<std::string> labels;
      for (const auto& label_value : *label_values) {
        labels.push_back(label_value.GetString());
      }
      mojo_descriptor_a->labels = std::move(labels);
      mojo_descriptor_a_list.push_back(std::move(mojo_descriptor_a));
    }
  }
  auto mojo_descriptors =
      side_panel::customize_chrome::mojom::Descriptors::New();
  mojo_descriptors->descriptor_a = std::move(mojo_descriptor_a_list);
  std::vector<side_panel::customize_chrome::mojom::DescriptorBPtr>
      mojo_descriptor_b_list;
  if (descriptor_b) {
    for (const auto& descriptor : *descriptor_b) {
      const base::Value::Dict& descriptor_b_dict = descriptor.GetDict();
      auto* label = descriptor_b_dict.FindString("label");
      auto* image_path = descriptor_b_dict.FindString("image");
      if (!label || !image_path) {
        continue;
      }
      auto mojo_descriptor_b =
          side_panel::customize_chrome::mojom::DescriptorB::New();
      mojo_descriptor_b->label = *label;
      mojo_descriptor_b->image_path =
          base::StrCat({kDescriptorsBaseUrl, *image_path});
      mojo_descriptor_b_list.push_back(std::move(mojo_descriptor_b));
    }
  }
  mojo_descriptors->descriptor_b = std::move(mojo_descriptor_b_list);
  std::vector<std::string> mojo_descriptor_c_labels;
  if (descriptor_c_labels) {
    for (const auto& label_value : *descriptor_c_labels) {
      mojo_descriptor_c_labels.push_back(label_value.GetString());
    }
  }
  mojo_descriptors->descriptor_c = std::move(mojo_descriptor_c_labels);
  std::move(callback).Run(std::move(mojo_descriptors));
}

void WallpaperSearchHandler::OnWallpaperSearchResultsRetrieved(
    GetWallpaperSearchResultsCallback callback,
    optimization_guide::OptimizationGuideModelExecutionResult result) {
  if (!result.has_value()) {
    return;
  }
  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::WallpaperSearchResponse>(result.value());
  if (response->images().empty()) {
    return;
  }
  auto barrier = base::BarrierCallback<SkBitmap>(
      response->images_size(),
      base::BindOnce(&WallpaperSearchHandler::OnWallpaperSearchResultsDecoded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  // Decode each image that is sent back for security purposes. Switched them
  // from gfx::Image to SkBitmap before passing to the barrier callback because
  // of some issues with const gfx::Image& and base::BarrierCallback.
  for (auto& image : response->images()) {
    image_decoder_->DecodeImage(
        image.encoded_image(), gfx::Size(), nullptr,
        base::BindOnce(
            [](base::RepeatingCallback<void(SkBitmap)> barrier,
               const gfx::Image& image) {
              std::move(barrier).Run(image.AsBitmap());
            },
            barrier));
  }
}

// Save the full sized bitmaps and create a much smaller image version of each
// for sending back to the UI through the callback. Re-encode the bitmap and
// make it base64 for easy reading by the UI.
void WallpaperSearchHandler::OnWallpaperSearchResultsDecoded(
    GetWallpaperSearchResultsCallback callback,
    std::vector<SkBitmap> bitmaps) {
  std::vector<side_panel::customize_chrome::mojom::WallpaperSearchResultPtr>
      thumbnails;

  for (auto& bitmap : bitmaps) {
    auto dimensions =
        CalculateResizeDimensions(bitmap.width(), bitmap.height(), 100);
    SkBitmap small_bitmap = skia::ImageOperations::Resize(
        bitmap, skia::ImageOperations::RESIZE_GOOD,
        /* width */ dimensions.first,
        /* height */ dimensions.second);
    std::vector<unsigned char> encoded;
    const bool success = gfx::PNGCodec::EncodeBGRASkBitmap(
        small_bitmap, /*discard_transparency=*/false, &encoded);
    if (success) {
      auto thumbnail =
          side_panel::customize_chrome::mojom::WallpaperSearchResult::New();
      auto id = base::Token::CreateRandom();
      wallpaper_search_results_[id] = std::move(bitmap);
      thumbnail->image = base::Base64Encode(encoded);
      thumbnail->id = std::move(id);
      thumbnails.push_back(std::move(thumbnail));
    }
  }

  std::move(callback).Run(std::move(thumbnails));
}
