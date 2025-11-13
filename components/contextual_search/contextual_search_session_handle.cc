// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/contextual_search_session_handle.h"

#include "base/memory/ptr_util.h"
#include "base/unguessable_token.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/lens/contextual_input.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"

namespace contextual_search {

ContextualSearchSessionHandle::ContextualSearchSessionHandle(
    base::WeakPtr<ContextualSearchService> service,
    const base::UnguessableToken& session_id)
    : service_(service), session_id_(session_id) {}

ContextualSearchSessionHandle::~ContextualSearchSessionHandle() {
  if (service_) {
    service_->ReleaseSession(session_id_);
  }
}

ContextualSearchContextController*
ContextualSearchSessionHandle::GetController() const {
  return service_ ? service_->GetSessionController(session_id_) : nullptr;
}

ContextualSearchMetricsRecorder*
ContextualSearchSessionHandle::GetMetricsRecorder() const {
  return service_ ? service_->GetSessionMetricsRecorder(session_id_) : nullptr;
}

void ContextualSearchSessionHandle::NotifySessionStarted() {
  if (auto* controller = GetController()) {
    controller->InitializeIfNeeded();
    if (auto* metrics_recorder = GetMetricsRecorder()) {
      metrics_recorder->NotifySessionStateChanged(
          contextual_search::SessionState::kSessionStarted);
    }
  }
}

void ContextualSearchSessionHandle::NotifySessionAbandoned() {
  if (auto* metrics_recorder = GetMetricsRecorder()) {
    metrics_recorder->NotifySessionStateChanged(
        contextual_search::SessionState::kSessionAbandoned);
  }
}

std::optional<lens::proto::LensOverlaySuggestInputs>
ContextualSearchSessionHandle::GetSuggestInputs() const {
  if (auto* controller = GetController()) {
    return controller->suggest_inputs();
  }
  return std::nullopt;
}

void ContextualSearchSessionHandle::AddFileContext(
    std::string file_mime_type,
    mojo_base::BigBuffer file_bytes,
    std::optional<lens::ImageEncodingOptions> image_options,
    AddFileContextCallback callback) {
  auto* context_controller = GetController();
  auto* metrics_recorder = GetMetricsRecorder();
  if (!context_controller) {
    return;
  }
  if (!metrics_recorder) {
    return;
  }
  base::UnguessableToken file_token = base::UnguessableToken::Create();

  lens::MimeType mime_type;

  if (file_mime_type.find("pdf") != std::string::npos) {
    mime_type = lens::MimeType::kPdf;
  } else if (file_mime_type.find("image") != std::string::npos) {
    mime_type = lens::MimeType::kImage;
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
  metrics_recorder->RecordFileSizeMetric(mime_type, file_bytes.size());
  context_controller->StartFileUploadFlow(file_token, std::move(input_data),
                                          std::move(image_options));
}

void ContextualSearchSessionHandle::StartTabContextUploadFlow(
    const base::UnguessableToken& file_token,
    std::unique_ptr<lens::ContextualInputData> contextual_input_data,
    std::optional<lens::ImageEncodingOptions> image_options) {
  if (auto* controller = GetController()) {
    controller->StartFileUploadFlow(
        file_token, std::move(contextual_input_data), image_options);
  }
}

bool ContextualSearchSessionHandle::DeleteFile(
    const base::UnguessableToken& file_token) {
  // It is possible to receive a call to delete a context before that context
  // has been created in the query controller. We queue all context tokens for
  // deletion at query submission time.
  if (auto* context_controller = GetController()) {
    const contextual_search::FileInfo* file_info =
        context_controller->GetFileInfo(file_token);

    if (file_info == nullptr) {
      return false;
    }
    lens::MimeType file_type =
        file_info ? file_info->mime_type : lens::MimeType::kUnknown;
    contextual_search::FileUploadStatus file_status =
        file_info ? file_info->upload_status
                  : contextual_search::FileUploadStatus::kNotUploaded;

    bool success = context_controller->DeleteFile(file_token);
    if (auto* metrics_recorder = GetMetricsRecorder()) {
      metrics_recorder->RecordFileDeletedMetrics(success, file_type,
                                                 file_status);
    }
    return success;
  }
  return false;
}

void ContextualSearchSessionHandle::ClearFiles() {
  if (auto* controller = GetController()) {
    controller->ClearFiles();
  }
}

GURL ContextualSearchSessionHandle::CreateSearchUrl(
    std::unique_ptr<contextual_search::ContextualSearchContextController::
                        CreateSearchUrlRequestInfo> search_url_request_info) {
  auto* context_controller = GetController();
  if (!context_controller) {
    return GURL();
  }

  auto* metrics_recorder = GetMetricsRecorder();
  if (!metrics_recorder) {
    return GURL();
  }

  metrics_recorder->NotifySessionStateChanged(
      contextual_search::SessionState::kQuerySubmitted);
  std::string query_text = search_url_request_info->query_text;
  metrics_recorder->NotifySessionStateChanged(
      contextual_search::SessionState::kNavigationOccurred);
  metrics_recorder->RecordQueryMetrics(
      query_text.size(), context_controller->num_files_in_request());
  return context_controller->CreateSearchUrl(
      std::move(search_url_request_info));
}

}  // namespace contextual_search
