// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_handler.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_tree.h"
#include "base/containers/span.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_omnibox_client.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/lens/contextual_input.h"
#include "components/lens/tab_contextualization_controller.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "content/public/browser/page_navigator.h"

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

ComposeboxHandler::ComposeboxHandler(
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler,
    std::unique_ptr<ComposeboxQueryController> query_controller,
    std::unique_ptr<ComposeboxMetricsRecorder> metrics_recorder,
    Profile* profile,
    content::WebContents* web_contents,
    MetricsReporter* metrics_reporter)
    : SearchboxHandler(
          std::move(pending_searchbox_handler),
          profile,
          web_contents,
          metrics_reporter,
          std::make_unique<OmniboxController>(
              /*view=*/nullptr,
              std::make_unique<composebox::ComposeboxOmniboxClient>(
                  profile,
                  web_contents,
                  this,
                  query_controller.get()))),
      query_controller_(std::move(query_controller)),
      metrics_recorder_(std::move(metrics_recorder)),
      web_contents_(web_contents),
      page_{std::move(pending_page)},
      handler_(this, std::move(pending_handler)) {
  query_controller_->AddObserver(this);

  autocomplete_controller_observation_.Observe(autocomplete_controller());
}

ComposeboxHandler::~ComposeboxHandler() {
  query_controller_->RemoveObserver(this);
  autocomplete_controller_observation_.Reset();
  // Even though these are owned by `SearchboxHandler` whose destructor would
  // have destroyed these anyways, they have to be deconstructed here because
  // they have a pointer to `query_controller_`.
  controller_ = nullptr;
  owned_controller_.reset();
}

void ComposeboxHandler::NotifySessionStarted() {
  query_controller_->NotifySessionStarted();
  metrics_recorder_->NotifySessionStateChanged(SessionState::kSessionStarted);
}

void ComposeboxHandler::NotifySessionAbandoned() {
  query_controller_->NotifySessionAbandoned();
  metrics_recorder_->NotifySessionStateChanged(SessionState::kSessionAbandoned);
}

void ComposeboxHandler::SubmitQuery(
    const std::string& query_text,
    WindowOpenDisposition disposition,
    std::map<std::string, std::string> additional_params) {
  // Update the query controller state to reflect any deleted contexts.
  std::erase_if(deleted_context_tokens_,
                [this](const base::UnguessableToken& context_token) {
                  ComposeboxQueryController::FileInfo* file_info =
                      query_controller_->GetFileInfo(context_token);

                  if (file_info == nullptr) {
                    return false;
                  }

                  lens::MimeType file_type = file_info
                                                 ? file_info->mime_type_
                                                 : lens::MimeType::kUnknown;
                  FileUploadStatus file_status =
                      file_info ? file_info->GetFileUploadStatus()
                                : FileUploadStatus::kNotUploaded;

                  bool success = query_controller_->DeleteFile(context_token);
                  metrics_recorder_->RecordFileDeletedMetrics(
                      success, file_type, file_status);

                  return success;
                });

  // This is the time that the user clicked the submit button, however optional
  // autocomplete logic may be run before this if there was a match associated
  // with the query.
  base::Time query_start_time = base::Time::Now();
  metrics_recorder_->NotifySessionStateChanged(SessionState::kQuerySubmitted);
  OpenUrl(query_controller_->CreateAimUrl(query_text, query_start_time,
                                          additional_params),
          disposition);
  metrics_recorder_->NotifySessionStateChanged(
      SessionState::kNavigationOccurred);
  metrics_recorder_->RecordQueryMetrics(
      query_text.size(), query_controller_->num_files_in_request());
}

void ComposeboxHandler::SubmitQuery(const std::string& query_text,
                                    uint8_t mouse_button,
                                    bool alt_key,
                                    bool ctrl_key,
                                    bool meta_key,
                                    bool shift_key) {
  const WindowOpenDisposition disposition = ui::DispositionFromClick(
      /*middle_button=*/mouse_button == 1, alt_key, ctrl_key, meta_key,
      shift_key);
  SubmitQuery(query_text, disposition, /*additional_params=*/{});
}

void ComposeboxHandler::FocusChanged(bool focused) {
  // Unimplemented. Currently the composebox session is tied to when it is
  // connected/disconnected from the DOM, so this is not needed.
}

void ComposeboxHandler::OpenUrl(GURL url,
                                const WindowOpenDisposition disposition) {
  content::OpenURLParams params(url, content::Referrer(), disposition,
                                ui::PAGE_TRANSITION_LINK, false);
  web_contents_->OpenURL(params, base::DoNothing());
}

void ComposeboxHandler::AddFileContext(
    composebox::mojom::SelectedFileInfoPtr file_info_mojom,
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
  metrics_recorder_->RecordFileSizeMetric(mime_type, file_bytes.size());
  query_controller_->StartFileUploadFlow(file_token, std::move(input_data),
                                         std::move(image_options));
}

void ComposeboxHandler::AddTabContext(int32_t tab_id,
                                      AddTabContextCallback callback) {
  const tabs::TabHandle handle = tabs::TabHandle(tab_id);
  tabs::TabInterface* const tab = handle.Get();
  if (!tab) {
    return;
  }

  RecordTabClickedMetric(tab);

  lens::TabContextualizationController* tab_contextualization_controller =
      tab->GetTabFeatures()->tab_contextualization_controller();
  auto token = base::UnguessableToken::Create();
  tab_contextualization_controller->GetPageContext(
      base::BindOnce(&ComposeboxHandler::OnGetTabPageContext,
                     weak_ptr_factory_.GetWeakPtr(), token));

  std::move(callback).Run(token);
}

void ComposeboxHandler::RecordTabClickedMetric(tabs::TabInterface* const tab) {
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

void ComposeboxHandler::DeleteContext(
    const base::UnguessableToken& context_token) {
  // It is possible to receive a call to delete a context before that context
  // has been created in the query controller. We queue all context tokens for
  // deletion at query submission time.
  deleted_context_tokens_.insert(context_token);
  query_controller_->ClearSuggestInputs();
}

void ComposeboxHandler::OnGetTabPageContext(
    const base::UnguessableToken& context_token,
    std::unique_ptr<lens::ContextualInputData> page_content_data) {
  query_controller_->StartFileUploadFlow(context_token,
                                         std::move(page_content_data),
                                         CreateImageEncodingOptions());
}

void ComposeboxHandler::ClearFiles() {
  deleted_context_tokens_.clear();
  query_controller_->ClearFiles();
}

void ComposeboxHandler::OnFileUploadStatusChanged(
    const base::UnguessableToken& file_token,
    lens::MimeType mime_type,
    composebox_query::mojom::FileUploadStatus file_upload_status,
    const std::optional<FileUploadErrorType>& error_type) {
  page_->OnContextualInputStatusChanged(file_token, file_upload_status,
                                        error_type);
  metrics_recorder_->OnFileUploadStatusChanged(mime_type, file_upload_status,
                                               error_type);
}

void ComposeboxHandler::ExecuteAction(uint8_t line,
                                      uint8_t action_index,
                                      const GURL& url,
                                      base::TimeTicks match_selection_timestamp,
                                      uint8_t mouse_button,
                                      bool alt_key,
                                      bool ctrl_key,
                                      bool meta_key,
                                      bool shift_key) {
  NOTREACHED();
}

void ComposeboxHandler::PopupElementSizeChanged(const gfx::Size& size) {
  NOTREACHED();
}

void ComposeboxHandler::OnThumbnailRemoved() {
  NOTREACHED();
}
