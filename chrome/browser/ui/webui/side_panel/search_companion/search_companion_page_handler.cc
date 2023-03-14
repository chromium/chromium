// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/search_companion/search_companion_page_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/zero_suggest_cache_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/side_panel/search_companion/search_companion_side_panel_ui.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/page_entities_metadata.pb.h"
#include "ui/base/resource/resource_bundle.h"

#include "chrome/common/chrome_isolated_world_ids.h"
#include "components/grit/components_resources.h"

using optimization_guide::proto::PageEntitiesMetadata;

constexpr base::TimeDelta kTimerInterval = base::Seconds(5);

namespace {
std::string ContentAnnotationsToString(
    const PageEntitiesMetadata& entities_metadata) {
  std::string content_annotation_string = "";
  for (const auto& category : entities_metadata.categories()) {
    if (category.category_id().empty()) {
      continue;
    }
    if (category.score() < 0 || category.score() > 100) {
      continue;
    }
    content_annotation_string +=
        "Page Category: " + category.category_id() + "\n";
  }
  for (auto& entity : entities_metadata.entities()) {
    if (entity.entity_id().empty()) {
      continue;
    }
    if (entity.score() < 0 || entity.score() > 100) {
      continue;
    }
    content_annotation_string += "Page Entity: " + entity.entity_id() + "\n";
  }
  return content_annotation_string;
}
}  // namespace

SearchCompanionPageHandler::SearchCompanionPageHandler(
    mojo::PendingReceiver<side_panel::mojom::SearchCompanionPageHandler>
        receiver,
    mojo::PendingRemote<side_panel::mojom::SearchCompanionPage> page,
    SearchCompanionSidePanelUI* search_companion_ui)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      search_companion_ui_(search_companion_ui),
      browser_(chrome::FindLastActive()),
      weak_ptr_factory_(this) {
  Profile* profile = Profile::FromWebUI(search_companion_ui->GetWebUi());
  zero_suggest_cache_service_observation_.Observe(
      ZeroSuggestCacheServiceFactory::GetForProfile(profile));

  if (optimization_guide::features::RemotePageMetadataEnabled()) {
    opt_guide_ = OptimizationGuideKeyedServiceFactory::GetForProfile(profile);

    if (opt_guide_) {
      opt_guide_->RegisterOptimizationTypes(
          {optimization_guide::proto::OptimizationType::PAGE_ENTITIES});
    }
  }
}

SearchCompanionPageHandler::~SearchCompanionPageHandler() = default;

void SearchCompanionPageHandler::OnZeroSuggestResponseUpdated(
    const std::string& page_url,
    const ZeroSuggestCacheService::CacheEntry& response) {
  NotifyUrlChanged(page_url);
  // response.substr() to crop initial characters: ")]}' "
  NotifyNewZeroSuggestPrefixData(response.response_json.substr(5));

  // Use zero suggest returning as the trigger to request entities from
  // optimization guide.
  // TODO(b/268285939): In the future use web navigation in the main frame to
  // trigger.
  if (opt_guide_) {
    opt_guide_->CanApplyOptimization(
        GURL(page_url),
        optimization_guide::proto::OptimizationType::PAGE_ENTITIES,
        base::BindOnce(
            &SearchCompanionPageHandler::HandleOptGuidePageEntitiesResponse,
            weak_ptr_factory_.GetWeakPtr()));
  }

  // Use zero suggest returning as the trigger to start a recurring timer to
  // fetch images from the main frame.
  // TODO(b/268285663): Rather than using a timer explore listening to page
  // scroll events.
  ExecuteFetchImagesJavascript();  // Fetching images one time right away
  fetch_images_timer_.Start(
      FROM_HERE, kTimerInterval, this,
      &SearchCompanionPageHandler::ExecuteFetchImagesJavascript);
}

void SearchCompanionPageHandler::HandleOptGuidePageEntitiesResponse(
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    return;
  }
  absl::optional<PageEntitiesMetadata> page_entities_metadata =
      metadata.ParsedMetadata<PageEntitiesMetadata>();
  if (page_entities_metadata) {
    NotifyNewOptimizationGuidePageAnnotations(
        ContentAnnotationsToString(*page_entities_metadata));
  }
}

void SearchCompanionPageHandler::ExecuteFetchImagesJavascript() {
  if (!browser_) {
    return;
  }

  content::RenderFrameHost* main_frame_render_host =
      browser_->tab_strip_model()
          ->GetActiveWebContents()
          ->GetPrimaryMainFrame();

  std::string script =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_SEARCH_COMPANION_FETCH_IMAGES_JS);

  main_frame_render_host->ExecuteJavaScriptInIsolatedWorld(
      base::UTF8ToUTF16(script),
      base::BindOnce(&SearchCompanionPageHandler::OnFetchImagesJavascriptResult,
                     weak_ptr_factory_.GetWeakPtr()),
      ISOLATED_WORLD_ID_CHROME_INTERNAL);
}

void SearchCompanionPageHandler::OnFetchImagesJavascriptResult(
    base::Value result) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      result.GetString(),
      base::BindOnce(
          &SearchCompanionPageHandler::OnImageFetchJsonSanitizationCompleted,
          weak_ptr_factory_.GetWeakPtr()));
}

void SearchCompanionPageHandler::OnImageFetchJsonSanitizationCompleted(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value() || !result.value().is_dict()) {
    return;
  }
  std::string new_image_content =
      *(result.value().GetDict().FindString("images"));
  NotifyNewViewportImages(new_image_content);
}

void SearchCompanionPageHandler::ShowUI() {
  if (auto embedder = search_companion_ui_->embedder()) {
    embedder->ShowUI();
  }
}

void SearchCompanionPageHandler::NotifyUrlChanged(std::string new_url) {
  page_->OnURLChanged(new_url);
}

void SearchCompanionPageHandler::NotifyNewZeroSuggestPrefixData(
    std::string suggest_response) {
  page_->OnNewZeroSuggestPrefixData(suggest_response);
}

void SearchCompanionPageHandler::NotifyNewOptimizationGuidePageAnnotations(
    std::string content_annotations) {
  page_->OnNewOptimizationGuidePageAnnotations(content_annotations);
}

void SearchCompanionPageHandler::NotifyNewViewportImages(
    std::string images_string) {
  page_->OnNewViewportImages(images_string);
}
