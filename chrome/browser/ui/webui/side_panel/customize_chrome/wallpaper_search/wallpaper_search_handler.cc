// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/wallpaper_search/wallpaper_search_handler.h"

#include <optional>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "base/barrier_callback.h"
#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/token.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_background_manager.h"
#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_data.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/webui/cr_components/theme_color_picker/customize_chrome_colors.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/wallpaper_search/wallpaper_search_string_map.h"
#include "chrome/common/chrome_features.h"
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
#include "components/optimization_guide/proto/model_quality_service.pb.h"
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
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"

using side_panel::customize_chrome::mojom::DescriptorDName;
using side_panel::customize_chrome::mojom::DescriptorDValue;
using side_panel::customize_chrome::mojom::UserFeedback;
using side_panel::customize_chrome::mojom::WallpaperSearchResult;
using side_panel::customize_chrome::mojom::WallpaperSearchResultPtr;
using side_panel::customize_chrome::mojom::WallpaperSearchStatus;

namespace {

const char kGstaticBaseURL[] =
    "https://www.gstatic.com/chrome-wallpaper-search/";
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

side_panel::customize_chrome::mojom::KeyLabelPtr MakeKeyLabel(
    const std::string& key,
    const std::string& label) {
  auto key_label = side_panel::customize_chrome::mojom::KeyLabel::New();
  key_label->key = key;
  key_label->label = label;
  return key_label;
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
    int64_t session_id,
    WallpaperSearchStringMap* string_map)
    : profile_(profile),
      data_decoder_(std::make_unique<data_decoder::DataDecoder>()),
      image_decoder_(*image_decoder),
      wallpaper_search_background_manager_(
          *wallpaper_search_background_manager),
      session_id_(session_id),
      string_map_(*string_map),
      client_(std::move(pending_client)),
      receiver_(this, std::move(pending_handler)) {
  wallpaper_search_background_manager_observation_.Observe(
      wallpaper_search_background_manager);
}

WallpaperSearchHandler::~WallpaperSearchHandler() {
  std::optional<base::Token> background_id;
  if (history_entry_) {
    background_id =
        wallpaper_search_background_manager_->SaveCurrentBackgroundToHistory(
            *history_entry_);
  }

  bool is_result = false;
  if (background_id) {
    if (base::Contains(wallpaper_search_results_, *background_id)) {
      base::UmaHistogramEnumeration(
          "NewTabPage.WallpaperSearch.SessionSetTheme",
          NtpWallpaperSearchThemeType::kResult);
      is_result = true;
    } else {
      base::UmaHistogramEnumeration(
          "NewTabPage.WallpaperSearch.SessionSetTheme",
          NtpWallpaperSearchThemeType::kHistory);
    }
  } else if (inspiration_token_ &&
             wallpaper_search_background_manager_->IsCurrentBackground(
                 *inspiration_token_)) {
    base::UmaHistogramEnumeration("NewTabPage.WallpaperSearch.SessionSetTheme",
                                  NtpWallpaperSearchThemeType::kInspiration);
  }

  if (!log_entries_.empty()) {
    auto& [log_entry, render_time] = log_entries_.back();
    auto* quality =
        log_entry
            ->quality_data<optimization_guide::WallpaperSearchFeatureTypeMap>();
    quality->set_final_request_in_session(true);
    if (render_time.has_value()) {
      quality->set_complete_latency_ms(
          (base::Time::Now() - *render_time).InMilliseconds());
    }
    if (is_result) {
      auto* image_quality =
          std::get<0>(wallpaper_search_results_[*background_id]);
      if (image_quality) {
        image_quality->set_selected(true);
      }
    }
    // Upload all the log entries once you set the final request.
    for (auto& entry : log_entries_) {
      optimization_guide::ModelQualityLogEntry::Upload(std::move(entry.first));
    }
  }
}

void WallpaperSearchHandler::GetDescriptors(GetDescriptorsCallback callback) {
  callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), nullptr);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "wallpaper_search_handler_descriptors_fetcher", R"(
        semantics {
          sender: "Customize Chrome"
          description:
            "This service downloads the strings and/or images of "
            "different search options for Customize Chrome's "
            "Wallpaper Search."
          trigger:
            "Opening Customize Chrome on the Desktop NTP, "
            "if Google is the default search provider "
            "and the user is signed in."
          data: "Sends the locale of the user, "
                "to ensure string localizations are correct."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "chrome-desktop-ntp@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2024-01-17"
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
      GURL(base::StrCat({kGstaticBaseURL, "descriptors_en-US.json"}));
  resource_request->request_initiator =
      url::Origin::Create(GURL(chrome::kChromeUINewTabURL));
  descriptors_simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  descriptors_simple_url_loader_->SetRetryOptions(
      /*max_retries=*/3,
      network::SimpleURLLoader::RetryMode::RETRY_ON_5XX |
          network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
          network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED);
  descriptors_simple_url_loader_->DownloadToString(
      profile_->GetURLLoaderFactory().get(),
      base::BindOnce(&WallpaperSearchHandler::OnDescriptorsRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      1024 * 1024);
}

void WallpaperSearchHandler::GetInspirations(GetInspirationsCallback callback) {
  callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                         std::nullopt);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "wallpaper_search_handler_inspirations_fetcher",
          R"(
        semantics {
          sender: "Customize Chrome"
          description:
            "This service downloads example images and their descriptions "
            "for Customize Chrome's Wallpaper Search."
          trigger:
            "Opening Customize Chrome on the Desktop NTP, "
            "if Google is the default search provider "
            "and the user is signed in."
          data: "Sends the locale of the user, "
                "to ensure string localizations are correct."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "chrome-desktop-ntp@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2024-01-17"
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
      GURL(base::StrCat({kGstaticBaseURL, "inspirations_en-US.json"}));
  resource_request->request_initiator =
      url::Origin::Create(GURL(chrome::kChromeUINewTabURL));

  inspirations_simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  inspirations_simple_url_loader_->SetRetryOptions(
      /*max_retries=*/3,
      network::SimpleURLLoader::RetryMode::RETRY_ON_5XX |
          network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
          network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED);
  inspirations_simple_url_loader_->DownloadToString(
      profile_->GetURLLoaderFactory().get(),
      base::BindOnce(&WallpaperSearchHandler::OnInspirationsRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      1024 * 1024);
}

void WallpaperSearchHandler::GetWallpaperSearchResults(
    side_panel::customize_chrome::mojom::ResultDescriptorsPtr
        result_descriptors,
    GetWallpaperSearchResultsCallback callback) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    std::move(callback).Run(WallpaperSearchStatus::kSignedOut,
                            std::vector<WallpaperSearchResultPtr>());
    return;
  }
#if BUILDFLAG(IS_CHROMEOS)
  // Check if user is browsing in guest mode.
  if (profile_->IsGuestSession()) {
    std::move(callback).Run(WallpaperSearchStatus::kSignedOut,
                            std::vector<WallpaperSearchResultPtr>());
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), WallpaperSearchStatus::kError,
      std::vector<WallpaperSearchResultPtr>());
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
  CHECK(result_descriptors->subject);
  descriptors.set_subject(*result_descriptors->subject);
  if (result_descriptors->style.has_value()) {
    descriptors.set_style(*result_descriptors->style);
  }
  if (result_descriptors->mood.has_value()) {
    descriptors.set_mood(*result_descriptors->mood);
  }
  if (result_descriptors->color) {
    if (result_descriptors->color->is_color()) {
      descriptors.set_color(
          skia::SkColorToHexString(result_descriptors->color->get_color()));
    } else if (result_descriptors->color->is_hue()) {
      descriptors.set_color(skia::SkColorToHexString(
          HueToSkColor(result_descriptors->color->get_hue())));
    }
  }
  optimization_guide_keyed_service->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kWallpaperSearch, request,
      base::BindOnce(&WallpaperSearchHandler::OnWallpaperSearchResultsRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     base::ElapsedTimer()));
}

void WallpaperSearchHandler::SetBackgroundToHistoryImage(
    const base::Token& result_id,
    side_panel::customize_chrome::mojom::ResultDescriptorsPtr descriptors) {
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
                         weak_ptr_factory_.GetWeakPtr(), result_id,
                         base::ElapsedTimer(), std::move(descriptors))));
}

void WallpaperSearchHandler::SetBackgroundToWallpaperSearchResult(
    const base::Token& result_id,
    double time,
    side_panel::customize_chrome::mojom::ResultDescriptorsPtr descriptors) {
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
  history_entry_ = std::make_unique<HistoryEntry>(result_id);
  history_entry_->subject = descriptors->subject;
  if (descriptors->style) {
    history_entry_->style = descriptors->style;
  }
  if (descriptors->mood) {
    history_entry_->mood = descriptors->mood;
  }
  wallpaper_search_background_manager_->SelectLocalBackgroundImage(
      result_id, bitmap, /*is_inspiration_image=*/false, base::ElapsedTimer());
}

void WallpaperSearchHandler::SetBackgroundToInspirationImage(
    const base::Token& id,
    const GURL& background_url) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "wallpaper_search_handler_inspiration_image_downloader", R"(
        semantics {
          sender: "Customize Chrome"
          description:
            "Downloads an image to customize Chrome's appearance "
            "i.e. change NTP background image and extract colors from the "
            "image to change Chrome's color."
          trigger:
            "Pressing an image under the category titled 'Inspiration' "
            "in Customize Chrome on the Desktop NTP, "
            "if Google is the default search provider "
            "and the user is signed in."
          data: "This request does not send any user data."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "chrome-desktop-ntp@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2024-01-17"
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
  resource_request->url = GURL(background_url);
  resource_request->request_initiator =
      url::Origin::Create(GURL(chrome::kChromeUINewTabURL));

  image_download_simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  image_download_simple_url_loader_->SetRetryOptions(
      /*max_retries=*/3,
      network::SimpleURLLoader::RetryMode::RETRY_ON_5XX |
          network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
          network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED);
  image_download_simple_url_loader_->DownloadToString(
      profile_->GetURLLoaderFactory().get(),
      base::BindOnce(&WallpaperSearchHandler::OnInspirationImageDownloaded,
                     weak_ptr_factory_.GetWeakPtr(), id, base::ElapsedTimer()),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void WallpaperSearchHandler::UpdateHistory() {
  const auto& history = wallpaper_search_background_manager_->GetHistory();
  std::vector<WallpaperSearchResultPtr> thumbnails;

  auto barrier = base::BarrierCallback<std::pair<SkBitmap, base::Token>>(
      history.size(), base::BindOnce(&WallpaperSearchHandler::OnHistoryDecoded,
                                     weak_ptr_factory_.GetWeakPtr(), history));

  for (const auto& entry : history) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(
            &ReadFile,
            profile_->GetPath().AppendASCII(
                entry.id.ToString() +
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
                           barrier, entry.id)));
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
            .first
            ->quality_data<optimization_guide::WallpaperSearchFeatureTypeMap>();
    if (quality) {
      quality->set_user_feedback(user_feedback);
    }
  }
}

void WallpaperSearchHandler::OpenHelpArticle() {
  NavigateParams navigate_params(profile_,
                                 GURL(chrome::kWallpaperSearchLearnMorePageURL),
                                 ui::PAGE_TRANSITION_LINK);
  navigate_params.window_action = NavigateParams::WindowAction::SHOW_WINDOW;
  navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&navigate_params);
}

void WallpaperSearchHandler::LaunchHatsSurvey() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WallpaperSearchHandler::LaunchDelayedHatsSurvey,
                     weak_ptr_factory_.GetWeakPtr()),
      base::GetFieldTrialParamByFeatureAsTimeDelta(
          features::kHappinessTrackingSurveysForWallpaperSearch,
          ntp_features::kWallpaperSearchHatsDelayParam, base::TimeDelta()));
}

void WallpaperSearchHandler::ShowFeedbackPage() {
#if BUILDFLAG(IS_CHROMEOS)
  if (skip_show_feedback_page_for_testing_) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }
  OptimizationGuideKeyedService* opt_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser->profile());
  if (!opt_guide_keyed_service ||
      !opt_guide_keyed_service->ShouldFeatureBeCurrentlyAllowedForFeedback(
          optimization_guide::proto::LogAiDataRequest::kWallpaperSearch)) {
    return;
  }
  base::Value::Dict feedback_metadata;
  if (!log_entries_.empty()) {
    feedback_metadata.Set("log_id", log_entries_.back()
                                        .first->log_ai_data_request()
                                        ->model_execution_info()
                                        .execution_id());
  }
  chrome::ShowFeedbackPage(
      browser, feedback::kFeedbackSourceAI,
      /*description_template=*/std::string(),
      /*description_placeholder_text=*/
      l10n_util::GetStringUTF8(IDS_NTP_WALLPAPER_SEARCH_FEEDBACK_PLACEHOLDER),
      /*category_tag=*/"wallpaper_search",
      /*extra_diagnostics=*/std::string(),
      /*autofill_metadata=*/base::Value::Dict(), std::move(feedback_metadata));
}

void WallpaperSearchHandler::OnHistoryUpdated() {
  WallpaperSearchHandler::UpdateHistory();
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
    DVLOG(1) << "Request failed with error: "
             << descriptors_simple_url_loader_->NetError();
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

  std::vector<side_panel::customize_chrome::mojom::GroupPtr> mojo_group_list;
  if (descriptor_a) {
    for (const auto& descriptor : *descriptor_a) {
      const base::Value::Dict& descriptor_a_dict = descriptor.GetDict();
      auto* category = descriptor_a_dict.FindString("category");
      auto* label_values = descriptor_a_dict.FindList("labels");
      if (!category || !label_values) {
        continue;
      }
      auto category_label = string_map_->FindCategory(*category);
      if (!category_label) {
        continue;
      }
      auto mojo_group = side_panel::customize_chrome::mojom::Group::New();
      mojo_group->category = *category_label;
      std::vector<side_panel::customize_chrome::mojom::KeyLabelPtr>
          mojo_descriptor_a_list;
      for (const auto& label_value : *label_values) {
        auto descriptor_a_label =
            string_map_->FindDescriptorA(label_value.GetString());
        if (!descriptor_a_label) {
          continue;
        }
        mojo_descriptor_a_list.push_back(
            MakeKeyLabel(label_value.GetString(), *descriptor_a_label));
      }
      mojo_group->descriptor_as = std::move(mojo_descriptor_a_list);
      mojo_group_list.push_back(std::move(mojo_group));
    }
  }
  auto mojo_descriptors =
      side_panel::customize_chrome::mojom::Descriptors::New();
  mojo_descriptors->groups = std::move(mojo_group_list);
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
      auto descriptor_b_label = string_map_->FindDescriptorB(*label);
      if (!descriptor_b_label) {
        continue;
      }
      mojo_descriptor_b->key = *label;
      mojo_descriptor_b->label = *descriptor_b_label;
      mojo_descriptor_b->image_path =
          base::StrCat({kGstaticBaseURL, *image_path});
      mojo_descriptor_b_list.push_back(std::move(mojo_descriptor_b));
    }
  }
  mojo_descriptors->descriptor_b = std::move(mojo_descriptor_b_list);
  std::vector<side_panel::customize_chrome::mojom::KeyLabelPtr>
      mojo_descriptor_c_list;
  if (descriptor_c_labels) {
    for (const auto& label_value : *descriptor_c_labels) {
      auto descriptor_c_label =
          string_map_->FindDescriptorC(label_value.GetString());
      if (!descriptor_c_label) {
        continue;
      }
      mojo_descriptor_c_list.push_back(
          MakeKeyLabel(label_value.GetString(), *descriptor_c_label));
    }
  }
  mojo_descriptors->descriptor_c = std::move(mojo_descriptor_c_list);
  std::move(callback).Run(std::move(mojo_descriptors));
}

void WallpaperSearchHandler::OnHistoryDecoded(
    std::vector<HistoryEntry> history,
    std::vector<std::pair<SkBitmap, base::Token>> results) {
  std::vector<WallpaperSearchResultPtr> thumbnails;

  // Use the original history array to order the results.
  // O(n^2) but there should never be more than 6 in each vector.
  for (const auto& entry : history) {
    for (auto& [bitmap, id] : results) {
      if (entry.id == id) {
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
          auto thumbnail = WallpaperSearchResult::New();
          thumbnail->image = base::Base64Encode(encoded);
          thumbnail->id = std::move(id);
          if (entry.subject) {
            thumbnail->descriptors =
                side_panel::customize_chrome::mojom::ResultDescriptors::New();
            thumbnail->descriptors->subject = *entry.subject;
            if (entry.style) {
              thumbnail->descriptors->style = *entry.style;
            }
            if (entry.mood) {
              thumbnail->descriptors->mood = *entry.mood;
            }
          }
          thumbnails.push_back(std::move(thumbnail));
        }
        break;
      }
    }
  }
  client_->SetHistory(std::move(thumbnails));
}

void WallpaperSearchHandler::OnInspirationImageDownloaded(
    const base::Token& id,
    base::ElapsedTimer timer,
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    // Network errors (i.e. the server did not provide a response).
    DVLOG(1) << "Request failed with error: "
             << image_download_simple_url_loader_->NetError();
    return;
  }
  image_decoder_->DecodeImage(
      *response_body, gfx::Size(), nullptr,
      base::BindOnce(&WallpaperSearchHandler::OnInspirationImageDecoded,
                     weak_ptr_factory_.GetWeakPtr(), id, std::move(timer)));
}

void WallpaperSearchHandler::OnInspirationImageDecoded(
    const base::Token& id,
    base::ElapsedTimer timer,
    const gfx::Image& image) {
  inspiration_token_ = id;
  wallpaper_search_background_manager_->SelectLocalBackgroundImage(
      id, image.AsBitmap(), /*is_inspiration_image=*/true, std::move(timer));
}

void WallpaperSearchHandler::OnInspirationsRetrieved(
    GetInspirationsCallback callback,
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    // Network errors (i.e. the server did not provide a response).
    DVLOG(1) << "Request failed with error: "
             << inspirations_simple_url_loader_->NetError();
    std::move(callback).Run(std::nullopt);
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
      base::BindOnce(&WallpaperSearchHandler::OnInspirationsJsonParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WallpaperSearchHandler::OnInspirationsJsonParsed(
    GetInspirationsCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value() || !result->is_list()) {
    DVLOG(1) << "Parsing JSON failed: " << result.error();
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::vector<side_panel::customize_chrome::mojom::InspirationGroupPtr>
      mojo_inspiration_groups;
  for (const auto& inspiration : result->GetList()) {
    if (!inspiration.is_dict()) {
      continue;
    }
    const base::Value::Dict& inspiration_dict = inspiration.GetDict();
    const base::Value::List* images = inspiration_dict.FindList("images");
    const std::string* descriptor_a =
        inspiration_dict.FindString("descriptor_a");
    if (!images || !descriptor_a) {
      continue;
    }
    auto descriptor_a_label = string_map_->FindDescriptorA(*descriptor_a);
    if (!descriptor_a_label) {
      continue;
    }
    auto mojo_inspiration_group =
        side_panel::customize_chrome::mojom::InspirationGroup::New();
    mojo_inspiration_group->descriptors =
        side_panel::customize_chrome::mojom::InspirationDescriptors::New();
    mojo_inspiration_group->descriptors->subject =
        MakeKeyLabel(*descriptor_a, *descriptor_a_label);
    if (const std::string* descriptor_b =
            inspiration_dict.FindString("descriptor_b")) {
      auto descriptor_b_label = string_map_->FindDescriptorB(*descriptor_b);
      if (!descriptor_b_label) {
        continue;
      }
      mojo_inspiration_group->descriptors->style =
          MakeKeyLabel(*descriptor_b, *descriptor_b_label);
    }
    if (const std::string* descriptor_c =
            inspiration_dict.FindString("descriptor_c")) {
      auto descriptor_c_label = string_map_->FindDescriptorC(*descriptor_c);
      if (!descriptor_c_label) {
        continue;
      }
      mojo_inspiration_group->descriptors->mood =
          MakeKeyLabel(*descriptor_c, *descriptor_c_label);
    }
    if (const base::Value::Dict* descriptor_d_dict =
            inspiration_dict.FindDict("descriptor_d")) {
      if (const std::string* descriptor_d_name =
              descriptor_d_dict->FindString("name")) {
        if (descriptor_d_name->compare("Yellow") == 0) {
          mojo_inspiration_group->descriptors->color =
              DescriptorDValue::NewName(DescriptorDName::kYellow);
        }
      }
    }
    std::vector<side_panel::customize_chrome::mojom::InspirationPtr>
        mojo_inspiration_list;
    for (const auto& image : *images) {
      const base::Value::Dict& image_dict = image.GetDict();
      const std::string* background_image =
          image_dict.FindString("background_image");
      const std::string* thumbnail_image =
          image_dict.FindString("thumbnail_image");
      const std::string* id_string = image_dict.FindString("id");
      if (!background_image || !thumbnail_image || !id_string) {
        continue;
      }
      auto description = string_map_->FindInspirationDescription(*id_string);
      if (!description) {
        continue;
      }
      const std::optional<base::Token> id_token =
          base::Token::FromString(*id_string);
      if (!id_token.has_value()) {
        continue;
      }
      auto mojo_inspiration =
          side_panel::customize_chrome::mojom::Inspiration::New();
      mojo_inspiration->id = id_token.value();
      mojo_inspiration->background_url =
          GURL(base::StrCat({kGstaticBaseURL, *background_image}));
      mojo_inspiration->thumbnail_url =
          GURL(base::StrCat({kGstaticBaseURL, *thumbnail_image}));
      mojo_inspiration->description = *description;
      mojo_inspiration_list.push_back(std::move(mojo_inspiration));
    }
    if (mojo_inspiration_list.size() > 0) {
      mojo_inspiration_group->inspirations = std::move(mojo_inspiration_list);
      mojo_inspiration_groups.push_back(std::move(mojo_inspiration_group));
    }
  }
  if (mojo_inspiration_groups.size() > 0) {
    std::move(callback).Run(std::move(mojo_inspiration_groups));
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

void WallpaperSearchHandler::SelectHistoryImage(
    const base::Token& id,
    base::ElapsedTimer timer,
    side_panel::customize_chrome::mojom::ResultDescriptorsPtr descriptors,
    const gfx::Image& image) {
  history_entry_ = std::make_unique<HistoryEntry>(id);
  if (descriptors->subject) {
    history_entry_->subject = descriptors->subject;
  }
  if (descriptors->style) {
    history_entry_->style = descriptors->style;
  }
  if (descriptors->mood) {
    history_entry_->mood = descriptors->mood;
  }
  wallpaper_search_background_manager_->SelectHistoryImage(id, image,
                                                           std::move(timer));
}

void WallpaperSearchHandler::OnWallpaperSearchResultsRetrieved(
    GetWallpaperSearchResultsCallback callback,
    base::ElapsedTimer request_timer,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  if (!log_entries_.empty()) {
    auto& [prev_log_entry, render_time] = log_entries_.back();
    if (render_time.has_value()) {
      prev_log_entry
          ->quality_data<optimization_guide::WallpaperSearchFeatureTypeMap>()
          ->set_complete_latency_ms(
              (base::Time::Now() - *render_time).InMilliseconds());
    }
  }
  if (log_entry) {
    // Clear out images in response to save bytes for logging.
    log_entry->log_ai_data_request()
        ->mutable_wallpaper_search()
        ->mutable_response()
        ->clear_images();
    log_entries_.emplace_back(std::move(log_entry), std::nullopt);
  }
  if (!log_entries_.empty()) {
    auto* quality =
        log_entries_.back()
            .first
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
      std::move(callback).Run(WallpaperSearchStatus::kRequestThrottled,
                              std::vector<WallpaperSearchResultPtr>());
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
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     base::ElapsedTimer()));

  // Decode each image that is sent back for security purposes. Switched them
  // from gfx::Image to SkBitmap before passing to the barrier callback because
  // of some issues with const gfx::Image& and base::BarrierCallback.
  for (auto& image : response->images()) {
    optimization_guide::proto::WallpaperSearchImageQuality* image_quality =
        nullptr;
    if (!log_entries_.empty()) {
      auto* quality =
          log_entries_.back()
              .first->quality_data<
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
        std::make_optional(base::Time::FromMillisecondsSinceUnixEpoch(time));
  }
  if (!log_entries_.empty()) {
    log_entries_.back().second =
        std::make_optional(base::Time::FromMillisecondsSinceUnixEpoch(time));
  }
}

// Save the full sized bitmaps and create a much smaller image version of each
// for sending back to the UI through the callback. Re-encode the bitmap and
// make it base64 for easy reading by the UI.
void WallpaperSearchHandler::OnWallpaperSearchResultsDecoded(
    GetWallpaperSearchResultsCallback callback,
    base::ElapsedTimer processing_timer,
    std::vector<
        std::pair<optimization_guide::proto::WallpaperSearchImageQuality*,
                  SkBitmap>> bitmaps) {
  std::vector<WallpaperSearchResultPtr> thumbnails;

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
      auto thumbnail = WallpaperSearchResult::New();
      auto id = base::Token::CreateRandom();
      wallpaper_search_results_[id] =
          std::make_tuple(image_quality, std::nullopt, std::move(bitmap));
      thumbnail->image = base::Base64Encode(encoded);
      thumbnail->id = std::move(id);
      thumbnails.push_back(std::move(thumbnail));
    }
  }

  UmaHistogramMediumTimes(
      "NewTabPage.WallpaperSearch.GetResultProcessingLatency",
      processing_timer.Elapsed());
  std::move(callback).Run(WallpaperSearchStatus::kOk, std::move(thumbnails));
}

void WallpaperSearchHandler::LaunchDelayedHatsSurvey() {
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);
  CHECK(hats_service);
  hats_service->LaunchSurvey(kHatsSurveyTriggerWallpaperSearch);
}
