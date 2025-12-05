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
  auto* controller = GetController();
  if (!controller) {
    return std::nullopt;
  }

  const auto& suggest_inputs =
      controller->CreateSuggestInputs(uploaded_context_tokens_);
  if (suggest_inputs->has_encoded_request_id()) {
    return *suggest_inputs.get();
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
  uploaded_context_tokens_.push_back(file_token);

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

void ContextualSearchSessionHandle::AddTabContext(
    int32_t tab_id,
    AddTabContextCallback callback) {
  // Create the file token and add it to the list of uploaded context tokens so
  // that it is referenced in the search url.
  base::UnguessableToken file_token = base::UnguessableToken::Create();
  uploaded_context_tokens_.push_back(file_token);
  // TODO(crbug.com/461869881): Store tab metadata in a list of attached tabs
  // to be able to return the list of attached tabs.
  std::move(callback).Run(file_token);
}

void ContextualSearchSessionHandle::StartTabContextUploadFlow(
    const base::UnguessableToken& file_token,
    std::unique_ptr<lens::ContextualInputData> contextual_input_data,
    std::optional<lens::ImageEncodingOptions> image_options) {
  // Exit early if the file token is not in the list of uploaded context
  // tokens, i.e. it was deleted before the upload flow could start.
  auto it = std::find(uploaded_context_tokens_.begin(),
                      uploaded_context_tokens_.end(), file_token);
  if (it == uploaded_context_tokens_.end()) {
    return;
  }

  if (auto* metrics_recorder = GetMetricsRecorder()) {
    auto mime_type = contextual_input_data->primary_content_type.value_or(
        lens::MimeType::kUnknown);
    size_t content_size = 0;
    if (contextual_input_data->context_input.has_value()) {
      for (const auto& input : *contextual_input_data->context_input) {
        content_size += input.bytes_.size();
      }
    }

    if (contextual_input_data->viewport_screenshot_bytes.has_value()) {
      content_size += contextual_input_data->viewport_screenshot_bytes->size();
    }

    if (contextual_input_data->viewport_screenshot.has_value()) {
      content_size +=
          contextual_input_data->viewport_screenshot->computeByteSize();
    }

    metrics_recorder->RecordFileSizeMetric(mime_type, content_size);
  }

  if (auto* controller = GetController()) {
    controller->StartFileUploadFlow(
        file_token, std::move(contextual_input_data), image_options);
  }
}

bool ContextualSearchSessionHandle::DeleteFile(
    const base::UnguessableToken& file_token) {
  // Remove the file token from the list of uploaded context tokens.
  auto it = std::find(uploaded_context_tokens_.begin(),
                      uploaded_context_tokens_.end(), file_token);
  if (it != uploaded_context_tokens_.end()) {
    uploaded_context_tokens_.erase(it);
  }

  // Also delete the file from the context controller if it exists.
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
  uploaded_context_tokens_.clear();
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
  metrics_recorder->RecordQueryMetrics(query_text.size(),
                                       uploaded_context_tokens_.size());
  search_url_request_info->file_tokens = uploaded_context_tokens_;
  return context_controller->CreateSearchUrl(
      std::move(search_url_request_info));
}

lens::ClientToAimMessage
ContextualSearchSessionHandle::CreateClientToAimRequest(
    std::unique_ptr<contextual_search::ContextualSearchContextController::
                        CreateClientToAimRequestInfo>
        create_client_to_aim_request_info) {
  auto* context_controller = GetController();
  if (!context_controller) {
    return lens::ClientToAimMessage();
  }

  // Move the uploaded tokens to the request's file_tokens.
  create_client_to_aim_request_info->file_tokens =
      std::exchange(uploaded_context_tokens_, {});

  // Copy the tokens from this request to the list of all submitted tokens.
  submitted_context_tokens_.insert(
      submitted_context_tokens_.end(),
      create_client_to_aim_request_info->file_tokens.begin(),
      create_client_to_aim_request_info->file_tokens.end());

  // TODO(crbug.com/463705266): Add metrics recording.

  return context_controller->CreateClientToAimRequest(
      std::move(create_client_to_aim_request_info));
}

std::vector<base::UnguessableToken>
ContextualSearchSessionHandle::GetUploadedContextTokens() const {
  return uploaded_context_tokens_;
}

std::vector<base::UnguessableToken>
ContextualSearchSessionHandle::GetSubmittedContextTokens() const {
  return submitted_context_tokens_;
}

void ContextualSearchSessionHandle::ClearSubmittedContextTokens() {
  submitted_context_tokens_.clear();
}

base::WeakPtr<ContextualSearchSessionHandle>
ContextualSearchSessionHandle::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace contextual_search
