// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/wallpaper_search/wallpaper_search_handler.h"

#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "base/barrier_callback.h"
#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/token.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_background_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/cr_components/theme_color_picker/customize_chrome_colors.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/wallpaper_search.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/prefs/pref_change_registrar.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"

using side_panel::customize_chrome::mojom::UserFeedback;

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

std::string ReadFile(const base::FilePath& path) {
  std::string result;
  base::ReadFileToString(path, &result);
  return result;
}

optimization_guide::proto::UserFeedback
OptimizationFeedbackFromWallpaperSearchFeedback(UserFeedback feedback) {
  switch (feedback) {
    case UserFeedback::kThumbsUp:
      return optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_UP;
    case UserFeedback::kThumbsDown:
      return optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_DOWN;
    case UserFeedback::kUnspecified:
      return optimization_guide::proto::UserFeedback::USER_FEEDBACK_UNSPECIFIED;
  }
}
}  // namespace

WallpaperSearchHandler::WallpaperSearchHandler(
    mojo::PendingReceiver<
        side_panel::customize_chrome::mojom::WallpaperSearchHandler>
        pending_handler,
    mojo::PendingRemote<
        side_panel::customize_chrome::mojom::WallpaperSearchClient>
        pending_client,
    Profile* profile,
    image_fetcher::ImageDecoder* image_decoder,
    WallpaperSearchBackgroundManager* wallpaper_search_background_manager,
    int64_t session_id)
    : profile_(profile),
      data_decoder_(std::make_unique<data_decoder::DataDecoder>()),
      image_decoder_(*image_decoder),
      wallpaper_search_background_manager_(
          *wallpaper_search_background_manager),
      session_id_(session_id),
      client_(std::move(pending_client)),
      receiver_(this, std::move(pending_handler)) {
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kNtpWallpaperSearchHistory,
      base::BindRepeating(&WallpaperSearchHandler::UpdateHistory,
                          weak_ptr_factory_.GetWeakPtr()));
}

WallpaperSearchHandler::~WallpaperSearchHandler() {
  auto backround_id =
      wallpaper_search_background_manager_->SaveCurrentBackgroundToHistory();
  if (!log_entries_.empty()) {
    auto* quality =
        log_entries_.back()
            ->quality_data<optimization_guide::WallpaperSearchFeatureTypeMap>();
    quality->set_final_request_in_session(true);
    if (backround_id.has_value() &&
        base::Contains(wallpaper_search_results_, *backround_id)) {
      auto* image_quality =
          std::get<0>(wallpaper_search_results_[*backround_id]);
      if (image_quality) {
        image_quality->set_selected(true);
      }
    }
  }
}

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
      side_panel::customize_chrome::mojom::WallpaperSearchStatus::kError,
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
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     base::ElapsedTimer()));
}

void WallpaperSearchHandler::SetBackgroundToHistoryImage(
    const base::Token& result_id) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(
          &ReadFile,
          profile_->GetPath().AppendASCII(
              result_id.ToString() +
              chrome::kChromeUIUntrustedNewTabPageBackgroundFilename)),
      base::BindOnce(
          &WallpaperSearchHandler::DecodeHistoryImage,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&WallpaperSearchHandler::SelectHistoryImage,
                         weak_ptr_factory_.GetWeakPtr(), result_id)));
}

void WallpaperSearchHandler::SetBackgroundToWallpaperSearchResult(
    const base::Token& result_id,
    double time) {
  CHECK(base::Contains(wallpaper_search_results_, result_id));
  auto& [image_quality, render_time, bitmap] =
      wallpaper_search_results_[result_id];
  if (image_quality) {
    image_quality->set_previewed(true);
    if (render_time.has_value()) {
      image_quality->set_preview_latency_ms(
          (base::Time::FromMillisecondsSinceUnixEpoch(time) - *render_time)
              .InMilliseconds());
    }
  }
  wallpaper_search_background_manager_->SelectLocalBackgroundImage(result_id,
                                                                   bitmap);
}

void WallpaperSearchHandler::UpdateHistory() {
  const auto& history = wallpaper_search_background_manager_->GetHistory();
  std::vector<side_panel::customize_chrome::mojom::WallpaperSearchResultPtr>
      thumbnails;

  auto barrier = base::BarrierCallback<std::pair<SkBitmap, base::Token>>(
      history.size(), base::BindOnce(&WallpaperSearchHandler::OnHistoryDecoded,
                                     weak_ptr_factory_.GetWeakPtr(), history));

  for (const auto& entry : history) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(
            &ReadFile,
            profile_->GetPath().AppendASCII(
                entry.ToString() +
                chrome::kChromeUIUntrustedNewTabPageBackgroundFilename)),
        base::BindOnce(&WallpaperSearchHandler::DecodeHistoryImage,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::BindOnce(
                           [](base::RepeatingCallback<void(
                                  std::pair<SkBitmap, base::Token>)> barrier,
                              const base::Token& id, const gfx::Image& image) {
                             std::move(barrier).Run(
                                 std::pair(image.AsBitmap(), id));
                           },
                           barrier, entry)));
  }
}

void WallpaperSearchHandler::SetUserFeedback(UserFeedback selected_option) {
  if (selected_option == UserFeedback::kThumbsDown) {
    ShowFeedbackPage();
  }

  optimization_guide::proto::UserFeedback user_feedback =
      OptimizationFeedbackFromWallpaperSearchFeedback(selected_option);
  if (!log_entries_.empty()) {
    auto* quality =
        log_entries_.back()
            ->quality_data<optimization_guide::WallpaperSearchFeatureTypeMap>();
    if (quality) {
      quality->set_user_feedback(user_feedback);
    }
  }
}

void WallpaperSearchHandler::ShowFeedbackPage() {
#if BUILDFLAG(IS_CHROMEOS)
  if (skip_show_feedback_page_for_testing_) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  base::Value::Dict feedback_metadata;
  if (!log_entries_.empty()) {
    feedback_metadata.Set("log_id", log_entries_.back()
                                        ->log_ai_data_request()
                                        ->mutable_model_execution_info()
                                        ->server_execution_id());
  }
  Browser* browser = chrome::FindLastActive();
  chrome::ShowFeedbackPage(
      browser, chrome::kFeedbackSourceAI,
      /*description_template=*/std::string(),
      /*description_placeholder_text=*/
      l10n_util::GetStringUTF8(IDS_NTP_WALLPAPER_SEARCH_FEEDBACK_PLACEHOLDER),
      /*category_tag=*/"wallpaper_search",
      /*extra_diagnostics=*/std::string(),
      /*autofill_metadata=*/base::Value::Dict(), std::move(feedback_metadata));
}

// This function is a wrapper around image_fetcher::ImageDecoder::DecodeImage()
// because of argument order when working with PostTaskAndReplyWithResult().
void WallpaperSearchHandler::DecodeHistoryImage(
    image_fetcher::ImageDecodedCallback callback,
    std::string image) {
  image_decoder_->DecodeImage(image, gfx::Size(), nullptr, std::move(callback));
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

void WallpaperSearchHandler::OnHistoryDecoded(
    std::vector<base::Token> history,
    std::vector<std::pair<SkBitmap, base::Token>> results) {
  std::vector<side_panel::customize_chrome::mojom::WallpaperSearchResultPtr>
      thumbnails;

  // Use the original history array to order the results.
  // O(n^2) but there should never be more than 6 in each vector.
  for (const auto& entry : history) {
    for (auto& [bitmap, id] : results) {
      if (entry == id) {
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
          thumbnail->image = base::Base64Encode(encoded);
          thumbnail->id = std::move(id);
          thumbnails.push_back(std::move(thumbnail));
        }
        break;
      }
    }
  }
  client_->SetHistory(std::move(thumbnails));
}

void WallpaperSearchHandler::SelectHistoryImage(const base::Token& id,
                                                const gfx::Image& image) {
  wallpaper_search_background_manager_->SelectHistoryImage(id, image);
}

void WallpaperSearchHandler::OnWallpaperSearchResultsRetrieved(
    GetWallpaperSearchResultsCallback callback,
    base::ElapsedTimer request_timer,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  if (log_entry) {
    log_entries_.push_back(std::move(log_entry));
  }
  if (!log_entries_.empty()) {
    auto* quality =
        log_entries_.back()
            ->quality_data<optimization_guide::WallpaperSearchFeatureTypeMap>();
    quality->set_session_id(session_id_);
    quality->set_index(log_entries_.size() - 1);
    // We will set this to true for the respective log entry when the side panel
    // closes.
    quality->set_final_request_in_session(false);
    quality->set_request_latency_ms(request_timer.Elapsed().InMilliseconds());
  }

  if (!result.has_value()) {
    if (result.error().error() ==
        optimization_guide::OptimizationGuideModelExecutionError::
            ModelExecutionError::kRequestThrottled) {
      std::move(callback).Run(
          side_panel::customize_chrome::mojom::WallpaperSearchStatus::
              kRequestThrottled,
          std::vector<
              side_panel::customize_chrome::mojom::WallpaperSearchResultPtr>());
    }
    return;
  }
  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::WallpaperSearchResponse>(result.value());
  if (response->images().empty()) {
    return;
  }
  auto barrier = base::BarrierCallback<std::pair<
      optimization_guide::proto::WallpaperSearchImageQuality*, SkBitmap>>(
      response->images_size(),
      base::BindOnce(&WallpaperSearchHandler::OnWallpaperSearchResultsDecoded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  // Decode each image that is sent back for security purposes. Switched them
  // from gfx::Image to SkBitmap before passing to the barrier callback because
  // of some issues with const gfx::Image& and base::BarrierCallback.
  for (auto& image : response->images()) {
    optimization_guide::proto::WallpaperSearchImageQuality* image_quality =
        nullptr;
    if (!log_entries_.empty()) {
      auto* quality =
          log_entries_.back()
              ->quality_data<
                  optimization_guide::WallpaperSearchFeatureTypeMap>();
      image_quality = quality->add_images_quality();
      image_quality->set_image_id(image.image_id());
      // We default to false and will flip if image was previewed or selected.
      image_quality->set_previewed(false);
      image_quality->set_selected(false);
    }
    image_decoder_->DecodeImage(
        image.encoded_image(), gfx::Size(), nullptr,
        base::BindOnce(
            [](base::RepeatingCallback<void(
                   std::pair<
                       optimization_guide::proto::WallpaperSearchImageQuality*,
                       SkBitmap>)> barrier,
               optimization_guide::proto::WallpaperSearchImageQuality*
                   image_quality,
               const gfx::Image& image) {
              std::move(barrier).Run(
                  std::make_pair(image_quality, image.AsBitmap()));
            },
            barrier, image_quality));
  }
}

void WallpaperSearchHandler::SetResultRenderTime(
    const std::vector<base::Token>& result_ids,
    double time) {
  for (const auto& id : result_ids) {
    CHECK(base::Contains(wallpaper_search_results_, id));
    auto& tuple = wallpaper_search_results_[id];
    std::get<1>(tuple) =
        absl::make_optional(base::Time::FromMillisecondsSinceUnixEpoch(time));
  }
}

// Save the full sized bitmaps and create a much smaller image version of each
// for sending back to the UI through the callback. Re-encode the bitmap and
// make it base64 for easy reading by the UI.
void WallpaperSearchHandler::OnWallpaperSearchResultsDecoded(
    GetWallpaperSearchResultsCallback callback,
    std::vector<
        std::pair<optimization_guide::proto::WallpaperSearchImageQuality*,
                  SkBitmap>> bitmaps) {
  std::vector<side_panel::customize_chrome::mojom::WallpaperSearchResultPtr>
      thumbnails;

  for (auto& [image_quality, bitmap] : bitmaps) {
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
      wallpaper_search_results_[id] =
          std::make_tuple(image_quality, absl::nullopt, std::move(bitmap));
      thumbnail->image = base::Base64Encode(encoded);
      thumbnail->id = std::move(id);
      thumbnails.push_back(std::move(thumbnail));
    }
  }

  std::move(callback).Run(
      side_panel::customize_chrome::mojom::WallpaperSearchStatus::kOk,
      std::move(thumbnails));
}
