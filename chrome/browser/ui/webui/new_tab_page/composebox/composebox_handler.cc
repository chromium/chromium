// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_handler.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_omnibox_client.h"
#include "components/lens/contextual_input.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/composebox/composebox_image_helper.h"
#include "content/public/browser/page_navigator.h"

using composebox::SessionState;

namespace {
class ComposeboxOmniboxClient final : public SearchboxOmniboxClient {
 public:
  ComposeboxOmniboxClient(Profile* profile,
                          content::WebContents* web_contents,
                          ComposeboxHandler* composebox_handler,
                          ComposeboxQueryController* query_controller);

  ~ComposeboxOmniboxClient() override;

  // OmniboxClient:
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override;

  void OnAutocompleteAccept(
      const GURL& destination_url,
      TemplateURLRef::PostContent* post_content,
      WindowOpenDisposition disposition,
      ui::PageTransition transition,
      AutocompleteMatchType::Type match_type,
      base::TimeTicks match_selection_timestamp,
      bool destination_url_entered_without_scheme,
      bool destination_url_entered_with_http_scheme,
      const std::u16string& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternative_nav_match) override;

 private:
  std::optional<lens::proto::LensOverlaySuggestInputs>
  GetLensOverlaySuggestInputs() const override;

  raw_ptr<ComposeboxHandler> composebox_handler_;
  raw_ptr<ComposeboxQueryController> query_controller_;
};

ComposeboxOmniboxClient::ComposeboxOmniboxClient(
    Profile* profile,
    content::WebContents* web_contents,
    ComposeboxHandler* composebox_handler,
    ComposeboxQueryController* query_controller)
    : SearchboxOmniboxClient(profile, web_contents),
      composebox_handler_(composebox_handler),
      query_controller_(query_controller) {}

ComposeboxOmniboxClient::~ComposeboxOmniboxClient() = default;

metrics::OmniboxEventProto::PageClassification
ComposeboxOmniboxClient::GetPageClassification(bool is_prefetch) const {
  return metrics::OmniboxEventProto::NTP_COMPOSEBOX;
}

void ComposeboxOmniboxClient::OnAutocompleteAccept(
    const GURL& destination_url,
    TemplateURLRef::PostContent* post_content,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    AutocompleteMatchType::Type match_type,
    base::TimeTicks match_selection_timestamp,
    bool destination_url_entered_without_scheme,
    bool destination_url_entered_with_http_scheme,
    const std::u16string& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternative_nav_match) {
  composebox_handler_->SubmitQuery(base::UTF16ToUTF8(text), disposition);
}

std::optional<lens::proto::LensOverlaySuggestInputs>
ComposeboxOmniboxClient::GetLensOverlaySuggestInputs() const {
  const auto& suggest_inputs = query_controller_->suggest_inputs();
  if (suggest_inputs.has_encoded_request_id()) {
    return suggest_inputs;
  }

  return std::nullopt;
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
      std::make_unique<ComposeboxOmniboxClient>(profile_, web_contents_, this,
                                                query_controller_.get()));
  controller_ = owned_controller_.get();

  autocomplete_controller_observation_.Observe(autocomplete_controller());
}

ComposeboxHandler::~ComposeboxHandler() {
  query_controller_->RemoveObserver(this);
  autocomplete_controller_observation_.Reset();
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

void ComposeboxHandler::SubmitQuery(const std::string& query_text,
                                    WindowOpenDisposition disposition) {
  // This is the time that the user clicked the submit button, however optional
  // autocomplete logic may be run before this if there was a match associated
  // with the query.
  base::Time query_start_time = base::Time::Now();
  metrics_recorder_->NotifySessionStateChanged(SessionState::kQuerySubmitted);
  OpenUrl(query_controller_->CreateAimUrl(query_text, query_start_time),
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
  SubmitQuery(query_text, disposition);
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

void ComposeboxHandler::AddFile(
    composebox::mojom::SelectedFileInfoPtr file_info_mojom,
    mojo_base::BigBuffer file_bytes,
    AddFileCallback callback) {
  base::UnguessableToken file_token = base::UnguessableToken::Create();

  std::optional<composebox::ImageEncodingOptions> image_options = std::nullopt;
  lens::MimeType mime_type;

  if ((file_info_mojom->mime_type).find("pdf") != std::string::npos) {
    mime_type = lens::MimeType::kPdf;
  } else if ((file_info_mojom->mime_type).find("image") != std::string::npos) {
    mime_type = lens::MimeType::kImage;
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
