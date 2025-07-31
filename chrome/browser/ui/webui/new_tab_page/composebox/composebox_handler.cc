// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_handler.h"

#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_omnibox_client.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/composebox/composebox_image_helper.h"
#include "content/public/browser/page_navigator.h"

namespace {
class ComposeboxOmniboxClient final : public SearchboxOmniboxClient {
 public:
  ComposeboxOmniboxClient(Profile* profile, content::WebContents* web_contents);
  ~ComposeboxOmniboxClient() override;

  // OmniboxClient:
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override;
};

ComposeboxOmniboxClient::ComposeboxOmniboxClient(
    Profile* profile,
    content::WebContents* web_contents)
    : SearchboxOmniboxClient(profile, web_contents) {}

ComposeboxOmniboxClient::~ComposeboxOmniboxClient() = default;

metrics::OmniboxEventProto::PageClassification
ComposeboxOmniboxClient::GetPageClassification(bool is_prefetch) const {
  // TODO(crbug.com/434711904): Create new page classification
  return metrics::OmniboxEventProto::NTP_REALBOX;
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
    : SearchboxHandler(std::move(pending_searchbox_handler),
                       profile,
                       web_contents,
                       metrics_reporter),
      query_controller_(std::move(query_controller)),
      metrics_recorder_(std::move(metrics_recorder)),
      web_contents_(web_contents),
      page_{std::move(pending_page)},
      handler_(this, std::move(pending_handler)) {
  query_controller_->AddObserver(this);

  // TODO(crbug.com/435470637): Consider moving to SearchboxHandler base class.
  owned_controller_ = std::make_unique<OmniboxController>(
      /*view=*/nullptr,
      std::make_unique<ComposeboxOmniboxClient>(profile_, web_contents_));
  controller_ = owned_controller_.get();

  autocomplete_controller_observation_.Observe(autocomplete_controller());
}

ComposeboxHandler::~ComposeboxHandler() {
  query_controller_->RemoveObserver(this);
}

void ComposeboxHandler::NotifySessionStarted() {
  query_controller_->NotifySessionStarted();
  metrics_recorder_->NotifySessionStateChanged(SessionState::kSessionStarted);
}

void ComposeboxHandler::NotifySessionAbandoned() {
  query_controller_->NotifySessionAbandoned();
  metrics_recorder_->NotifySessionStateChanged(SessionState::kSessionAbandoned);
}

void ComposeboxHandler::SubmitQuery(const std::string& query_text,
                                    uint8_t mouse_button,
                                    bool alt_key,
                                    bool ctrl_key,
                                    bool meta_key,
                                    bool shift_key) {
  // This is the time that the user clicked the submit button, and should not
  // go any lower in this method.
  base::Time query_start_time = base::Time::Now();
  metrics_recorder_->NotifySessionStateChanged(SessionState::kQuerySubmitted);
  const WindowOpenDisposition disposition = ui::DispositionFromClick(
      /*middle_button=*/mouse_button == 1, alt_key, ctrl_key, meta_key,
      shift_key);
  OpenUrl(query_controller_->CreateAimUrl(query_text, query_start_time), disposition);
  metrics_recorder_->NotifySessionStateChanged(
      SessionState::kNavigationOccurred);
  metrics_recorder_->RecordQueryMetrics(
      query_text.size(), query_controller_->num_files_in_request());
}

void ComposeboxHandler::OpenUrl(GURL url,
                                const WindowOpenDisposition disposition) {
  content::OpenURLParams params(url, content::Referrer(), disposition,
                                ui::PAGE_TRANSITION_LINK, false);
  web_contents_->OpenURL(params, base::DoNothing());
}

void ComposeboxHandler::AddFile(
    composebox::mojom::SelectedFileInfoPtr file_info_mojom,
    mojo_base::BigBuffer file_bytes,
    AddFileCallback callback) {
  scoped_refptr<base::RefCountedBytes> file_data =
      base::MakeRefCounted<base::RefCountedBytes>(file_bytes);

  auto file_info_metadata =
      std::make_unique<ComposeboxQueryController::FileInfo>();
  file_info_metadata->file_name = file_info_mojom->file_name;
  file_info_metadata->file_size_bytes = file_bytes.size();
  file_info_metadata->webui_selection_time = file_info_mojom->selection_time;
  file_info_metadata->file_token_ = base::UnguessableToken::Create();

  std::optional<composebox::ImageEncodingOptions> image_options = std::nullopt;

  if ((file_info_mojom->mime_type).find("pdf") != std::string::npos) {
    file_info_metadata->mime_type_ = lens::MimeType::kPdf;
  } else if ((file_info_mojom->mime_type).find("image") != std::string::npos) {
    file_info_metadata->mime_type_ = lens::MimeType::kImage;
    auto image_upload_config =
        ntp_composebox::FeatureConfig::Get().config.composebox().image_upload();
    image_options = composebox::ImageEncodingOptions{
        .enable_webp_encoding = image_upload_config.enable_webp_encoding(),
        .max_size = image_upload_config.downscale_max_image_size(),
        .max_height = image_upload_config.downscale_max_image_height(),
        .max_width = image_upload_config.downscale_max_image_width(),
        .compression_quality = image_upload_config.image_compression_quality()};
  } else {
    NOTREACHED();
  }

  std::move(callback).Run(file_info_metadata->file_token_);
  metrics_recorder_->RecordFileSizeMetric(file_info_metadata->mime_type_,
                                          file_bytes.size());
  query_controller_->StartFileUploadFlow(std::move(file_info_metadata),
                                         std::move(file_data),
                                         std::move(image_options));
}

void ComposeboxHandler::DeleteFile(const base::UnguessableToken& file_token) {
  ComposeboxQueryController::FileInfo* file_info =
      query_controller_->GetFileInfo(file_token);
  lens::MimeType file_type =
      file_info ? file_info->mime_type_ : lens::MimeType::kUnknown;
  FileUploadStatus file_status = file_info ? file_info->GetFileUploadStatus()
                                           : FileUploadStatus::kNotUploaded;

  // If an UnguessabledToken that wasn't in the cache was sent, delete fails.
  // Report a bad message.
  bool success = query_controller_->DeleteFile(file_token);
  metrics_recorder_->RecordFileDeletedMetrics(success, file_type, file_status);
  if (!success) {
    handler_.ReportBadMessage("An invalid file token was sent to DeleteFile");
  }
}

void ComposeboxHandler::ClearFiles() {
  query_controller_->ClearFiles();
}

void ComposeboxHandler::OnFileUploadStatusChanged(
    const base::UnguessableToken& file_token,
    lens::MimeType mime_type,
    composebox_query::mojom::FileUploadStatus file_upload_status,
    const std::optional<FileUploadErrorType>& error_type) {
  page_->OnFileUploadStatusChanged(file_token, file_upload_status, error_type);
  metrics_recorder_->OnFileUploadStatusChanged(mime_type, file_upload_status,
                                               error_type);
}

void ComposeboxHandler::DeleteAutocompleteMatch(uint8_t line, const GURL& url) {
  NOTREACHED();
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
