// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_handler.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/lens/contextual_input.h"
#include "components/lens/tab_contextualization_controller.h"
#include "components/omnibox/browser/vector_icons.h"

using composebox::SessionState;

namespace {

std::optional<lens::ImageEncodingOptions> CreateImageEncodingOptions() {
  auto image_upload_config =
      ntp_composebox::FeatureConfig::Get().config.composebox().image_upload();
  return lens::ImageEncodingOptions{
      .enable_webp_encoding = image_upload_config.enable_webp_encoding(),
      .max_size = image_upload_config.downscale_max_image_size(),
      .max_height = image_upload_config.downscale_max_image_height(),
      .max_width = image_upload_config.downscale_max_image_width(),
      .compression_quality = image_upload_config.image_compression_quality()};
}

}  // namespace

ContextualSearchboxHandler::ContextualSearchboxHandler(
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler,
    Profile* profile,
    content::WebContents* web_contents,
    MetricsReporter* metrics_reporter,
    std::unique_ptr<ComposeboxMetricsRecorder> composebox_metrics_recorder,
    std::unique_ptr<OmniboxController> controller,
    std::unique_ptr<ComposeboxQueryController> query_controller)
    : SearchboxHandler(
          std::move(pending_searchbox_handler),
          profile,
          web_contents,
          metrics_reporter,
          std::move(controller)),
      query_controller_(std::move(query_controller)),
      composebox_metrics_recorder_(std::move(composebox_metrics_recorder)),
      web_contents_(web_contents) {
  if (query_controller_) {
    file_upload_status_observer_.Observe(query_controller_.get());
  }
}

ContextualSearchboxHandler::~ContextualSearchboxHandler() = default;

void ContextualSearchboxHandler::NotifySessionStarted() {
  query_controller_->NotifySessionStarted();
  composebox_metrics_recorder_->NotifySessionStateChanged(
      SessionState::kSessionStarted);
}

void ContextualSearchboxHandler::NotifySessionAbandoned() {
  query_controller_->NotifySessionAbandoned();
  composebox_metrics_recorder_->NotifySessionStateChanged(
      SessionState::kSessionAbandoned);
}

void ContextualSearchboxHandler::AddFileContext(
    searchbox::mojom::SelectedFileInfoPtr file_info_mojom,
    mojo_base::BigBuffer file_bytes,
    AddFileContextCallback callback) {
  base::UnguessableToken file_token = base::UnguessableToken::Create();

  std::optional<lens::ImageEncodingOptions> image_options = std::nullopt;
  lens::MimeType mime_type;

  if ((file_info_mojom->mime_type).find("pdf") != std::string::npos) {
    mime_type = lens::MimeType::kPdf;
  } else if ((file_info_mojom->mime_type).find("image") != std::string::npos) {
    mime_type = lens::MimeType::kImage;
    image_options = CreateImageEncodingOptions();
  } else {
    NOTREACHED();
  }

  std::unique_ptr<lens::ContextualInputData> input_data =
      std::make_unique<lens::ContextualInputData>();
  input_data->context_input = std::vector<lens::ContextualInput>();
  input_data->primary_content_type = mime_type;

  base::span<const uint8_t> file_data_span = base::span(file_bytes);
  std::vector<uint8_t> file_data_vector(file_data_span.begin(),
                                        file_data_span.end());
  input_data->context_input->push_back(
      lens::ContextualInput(std::move(file_data_vector), mime_type));

  std::move(callback).Run(file_token);
  composebox_metrics_recorder_->RecordFileSizeMetric(mime_type,
                                                     file_bytes.size());
  query_controller_->StartFileUploadFlow(file_token, std::move(input_data),
                                         std::move(image_options));
}

void ContextualSearchboxHandler::AddTabContext(
    int32_t tab_id, AddTabContextCallback callback) {
  const tabs::TabHandle handle = tabs::TabHandle(tab_id);
  tabs::TabInterface* const tab = handle.Get();
  if (!tab) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  RecordTabClickedMetric(tab);

  lens::TabContextualizationController* tab_contextualization_controller =
      tab->GetTabFeatures()->tab_contextualization_controller();
  auto token = base::UnguessableToken::Create();
  tab_contextualization_controller->GetPageContext(
      base::BindOnce(&ContextualSearchboxHandler::OnGetTabPageContext,
                     weak_ptr_factory_.GetWeakPtr(), token));

  std::move(callback).Run(token);
}

void ContextualSearchboxHandler::RecordTabClickedMetric(
    tabs::TabInterface* const tab) {
  bool has_duplicate_title = false;
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_);
  if (browser_window_interface) {
    auto* tab_strip_model = browser_window_interface->GetTabStripModel();
    int tab_index = tab_strip_model->GetIndexOfTab(tab);
    if (tab_index != TabStripModel::kNoTab) {
      TabRendererData current_tab_renderer_data =
          TabRendererData::FromTabInModel(tab_strip_model, tab_index);
      const std::u16string& current_title = current_tab_renderer_data.title;

      int title_count = 0;
      for (int i = 0; i < tab_strip_model->count(); i++) {
        TabRendererData tab_renderer_data =
            TabRendererData::FromTabInModel(tab_strip_model, i);
        if (tab_renderer_data.title == current_title) {
          title_count++;
        }
      }
      if (title_count > 1) {
        has_duplicate_title = true;
      }
    }
  }

  UMA_HISTOGRAM_BOOLEAN("NewTabPage.Composebox.TabContextAdded", true);

  UMA_HISTOGRAM_BOOLEAN("NewTabPage.Composebox.TabWithDuplicateTitleClicked",
                        has_duplicate_title);
}

void ContextualSearchboxHandler::DeleteContext(
    const base::UnguessableToken& context_token) {
  // It is possible to receive a call to delete a context before that context
  // has been created in the query controller. We queue all context tokens for
  // deletion at query submission time.
  deleted_context_tokens_.insert(context_token);
  query_controller_->ClearSuggestInputs();
}

void ContextualSearchboxHandler::OnGetTabPageContext(
    const base::UnguessableToken& context_token,
    std::unique_ptr<lens::ContextualInputData> page_content_data) {
  query_controller_->StartFileUploadFlow(context_token,
                                         std::move(page_content_data),
                                         CreateImageEncodingOptions());
}

void ContextualSearchboxHandler::ClearFiles() {
  deleted_context_tokens_.clear();
  query_controller_->ClearFiles();
}

void ContextualSearchboxHandler::OnFileUploadStatusChanged(
    const base::UnguessableToken& file_token,
    lens::MimeType mime_type,
    composebox_query::mojom::FileUploadStatus file_upload_status,
    const std::optional<FileUploadErrorType>& error_type) {
  page_->OnContextualInputStatusChanged(file_token, file_upload_status,
                                        error_type);
  composebox_metrics_recorder_->OnFileUploadStatusChanged(
      mime_type, file_upload_status, error_type);
}

std::string ContextualSearchboxHandler::AutocompleteIconToResourceName(
    const gfx::VectorIcon& icon) {
  // The default icon for contextual suggestions is the subdirectory arrow right
  // icon. For the Lens composebox and realbox, we want to stay consistent with
  // the search loupe instead.
  if (icon.name == omnibox::kSubdirectoryArrowRightIcon.name) {
    return searchbox_internal::kSearchIconResourceName;
  }

  return SearchboxHandler::AutocompleteIconToResourceName(icon);
}
