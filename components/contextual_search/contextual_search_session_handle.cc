// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/contextual_search_session_handle.h"

#include <algorithm>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/ptr_util.h"
#include "base/unguessable_token.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/lens/contextual_input.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/prefs/pref_service.h"
#include "contextual_search_context_controller.h"
#include "contextual_search_types.h"
#include "pref_names.h"

namespace contextual_search {

namespace {

std::vector<FileInfo> TokensToFileInfos(
    ContextualSearchContextController* controller,
    const std::vector<base::UnguessableToken>& tokens) {
  std::vector<FileInfo> file_infos;
  if (!controller) {
    return file_infos;
  }
  for (const auto& token : tokens) {
    const auto* file_info = controller->GetFileInfo(token);
    if (!file_info) {
      continue;
    }
    file_infos.push_back(*file_info);
  }
  return file_infos;
}

}  // namespace

ContextualSearchSessionHandle::ContextualSearchSessionHandle(
    base::WeakPtr<ContextualSearchService> service,
    const base::UnguessableToken& session_id,
    std::optional<lens::LensOverlayInvocationSource> invocation_source)
    : service_(service),
      session_id_(session_id),
      invocation_source_(invocation_source) {}

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

bool ContextualSearchSessionHandle::CheckSearchContentSharingSettings(
    const PrefService* prefs) {
  if (!prefs) {
    return false;
  }
  policy_checked_ = true;
  return ContextualSearchService::IsContextSharingEnabled(prefs);
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

base::UnguessableToken ContextualSearchSessionHandle::CreateContextToken() {
  CHECK(policy_checked_);
  // Create the file token and add it to the list of uploaded context tokens so
  // that it is referenced in the query.
  base::UnguessableToken file_token = base::UnguessableToken::Create();
  uploaded_context_tokens_.push_back(file_token);
  return file_token;
}

void ContextualSearchSessionHandle::StartFileContextUploadFlow(
    const base::UnguessableToken& file_token,
    std::string file_name,
    std::string file_mime_type,
    mojo_base::BigBuffer file_bytes,
    std::optional<lens::ImageEncodingOptions> image_options) {
  // Exit early if the file token is not in the list of uploaded context
  // tokens, i.e. it was deleted before the upload flow could start.
  auto it = std::find(uploaded_context_tokens_.begin(),
                      uploaded_context_tokens_.end(), file_token);
  if (it == uploaded_context_tokens_.end()) {
    return;
  }

  auto* context_controller = GetController();
  auto* metrics_recorder = GetMetricsRecorder();
  if (!context_controller) {
    return;
  }
  if (!metrics_recorder) {
    return;
  }

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
  input_data->file_name = file_name;

  if (mime_type == lens::MimeType::kPdf){
    // For PDF file uploads, the file name is set in the page_title field.
    input_data->page_title = file_name;
  }

  base::span<const uint8_t> file_data_span = base::span(file_bytes);
  std::vector<uint8_t> file_data_vector(file_data_span.begin(),
                                        file_data_span.end());
  input_data->context_input->push_back(
      lens::ContextualInput(std::move(file_data_vector), mime_type));

  metrics_recorder->RecordFileSizeMetric(mime_type, file_bytes.size());
  context_controller->StartFileUploadFlow(file_token, std::move(input_data),
                                          std::move(image_options));
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

void ContextualSearchSessionHandle::StartModalityChipUploadFlow(
    const base::UnguessableToken& file_token,
    std::unique_ptr<lens::ModalityChipProps> modality_chip_props) {
  // Exit early if the file token is not in the list of uploaded context
  // tokens, i.e. it was deleted before the upload flow could start.
  auto it = std::find(uploaded_context_tokens_.begin(),
                      uploaded_context_tokens_.end(), file_token);
  if (it == uploaded_context_tokens_.end()) {
    return;
  }

  auto* context_controller = GetController();
  auto* metrics_recorder = GetMetricsRecorder();
  if (!context_controller) {
    return;
  }
  if (!metrics_recorder) {
    return;
  }

  // TODO(crbug.com/483820565): Add UMA logging for modality chips to the
  // metrics recorder.
  std::unique_ptr<lens::ContextualInputData> input_data =
      std::make_unique<lens::ContextualInputData>();
  // Create an input data with the modality chip props and no other data.
  input_data->modality_chip_props = std::move(*modality_chip_props);
  context_controller->StartFileUploadFlow(file_token, std::move(input_data),
                                          /*image_options=*/std::nullopt);
}

bool ContextualSearchSessionHandle::DeleteFile(
    const base::UnguessableToken& file_token) {
  // If the file was already submitted, don't delete it from the context
  // controller, so that the file info can be looked up in the future.
  // This prevents the file info for submitted content from being deleted
  // prematurely, such as when controlling the auto-tab chip for the transition
  // from the LensOverlay searchbox to the contextual tasks page.
  if (std::find(submitted_context_tokens_.begin(),
                submitted_context_tokens_.end(),
                file_token) != submitted_context_tokens_.end()) {
    return false;
  }

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

void ContextualSearchSessionHandle::CreateSearchUrl(
    std::unique_ptr<contextual_search::ContextualSearchContextController::
                        CreateSearchUrlRequestInfo> search_url_request_info,
    base::OnceCallback<void(GURL)> callback) {
  auto* context_controller = GetController();
  if (!context_controller) {
    std::move(callback).Run(GURL());
    return;
  }

  auto* metrics_recorder = GetMetricsRecorder();
  if (!metrics_recorder) {
    std::move(callback).Run(GURL());
    return;
  }

  auto uploaded_file_infos = GetUploadedContextFileInfos();
  NotifyQuerySubmittedSessionState(uploaded_file_infos);
  std::string query_text = search_url_request_info->query_text;
  metrics_recorder->NotifySessionStateChanged(
      contextual_search::SessionState::kNavigationOccurred);
  metrics_recorder->RecordQueryMetrics(query_text.size(),
                                       uploaded_context_tokens_.size());

  // If the request info has no file tokens, move the uploaded tokens to the
  // request. Otherwise, keep the file tokens as is and remove them from the
  // uploaded context tokens manually.
  if (search_url_request_info->file_tokens.empty()) {
    search_url_request_info->file_tokens =
        std::exchange(uploaded_context_tokens_, {});
  } else {
    for (const auto& token : search_url_request_info->file_tokens) {
      std::erase(uploaded_context_tokens_, token);
    }
  }

  // Copy the tokens from this request to the list of all submitted tokens.
  submitted_context_tokens_.insert(submitted_context_tokens_.end(),
                                   search_url_request_info->file_tokens.begin(),
                                   search_url_request_info->file_tokens.end());

  // Set the invocation source on the search URL request info, if it is not
  // already set.
  if (!search_url_request_info->invocation_source.has_value()) {
    search_url_request_info->invocation_source = invocation_source_;
  }

  context_controller->CreateSearchUrl(std::move(search_url_request_info),
                                      std::move(callback));
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

  // Move the uploaded tokens to the request's file_tokens. Make sure to dedupe
  // the tokens with those already in the ClientToAimRequestInfo.
  base::flat_set<base::UnguessableToken> file_tokens_set(
      std::move(create_client_to_aim_request_info->file_tokens));
  auto uploaded_tokens = std::exchange(uploaded_context_tokens_, {});
  file_tokens_set.insert(uploaded_tokens.begin(), uploaded_tokens.end());
  create_client_to_aim_request_info->file_tokens =
      std::move(file_tokens_set).extract();

  // Copy the tokens from this request to the list of all submitted tokens.
  submitted_context_tokens_.insert(
      submitted_context_tokens_.end(),
      create_client_to_aim_request_info->file_tokens.begin(),
      create_client_to_aim_request_info->file_tokens.end());

  if (auto* metrics_recorder = GetMetricsRecorder()) {
    auto uploaded_file_infos = GetSubmittedContextFileInfos();
    NotifyQuerySubmittedSessionState(uploaded_file_infos);
    std::string query_text = create_client_to_aim_request_info->query_text;
    metrics_recorder->RecordQueryMetrics(
        query_text.size(),
        create_client_to_aim_request_info->file_tokens.size());
  }

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

std::vector<FileInfo>
ContextualSearchSessionHandle::GetUploadedContextFileInfos() const {
  return TokensToFileInfos(GetController(), uploaded_context_tokens_);
}

std::vector<FileInfo>
ContextualSearchSessionHandle::GetSubmittedContextFileInfos() const {
  return TokensToFileInfos(GetController(), submitted_context_tokens_);
}

void ContextualSearchSessionHandle::ClearSubmittedContextTokens() {
  submitted_context_tokens_.clear();
}

void ContextualSearchSessionHandle::set_submitted_context_tokens(
    const std::vector<base::UnguessableToken>& tokens) {
  submitted_context_tokens_ = tokens;
}

bool ContextualSearchSessionHandle::IsTabInContext(SessionID session_id) const {
  ContextualSearchContextController* controller = GetController();
  if (!controller) {
    return false;
  }

  // TODO(crbug.com/468453630): The context needs to actually be populated
  // with tab data from the server-managed context list.
  for (const auto& file_info : GetSubmittedContextFileInfos()) {
    if (file_info.tab_session_id.has_value() &&
        file_info.tab_session_id.value() == session_id) {
      return true;
    }
  }
  return false;
}

base::WeakPtr<ContextualSearchSessionHandle>
ContextualSearchSessionHandle::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ContextualSearchSessionHandle::NotifyQuerySubmittedSessionState(
    const std::vector<FileInfo>& file_infos) {
  if (auto* metrics_recorder = GetMetricsRecorder()) {
    bool has_tab_context = false;
    bool has_non_tab_context = false;
    for (const auto& file_info : file_infos) {
      if (file_info.tab_url.has_value()) {
        has_tab_context = true;
      } else {
        has_non_tab_context = true;
      }
    }
    metrics_recorder->NotifyQuerySubmitted(has_tab_context,
                                           has_non_tab_context);
  }
}

}  // namespace contextual_search
