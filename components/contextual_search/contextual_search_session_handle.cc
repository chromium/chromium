// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/contextual_search_session_handle.h"

#include <algorithm>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/unguessable_token.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/query_contextualizer.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_features.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/omnibox/common/composebox_features.h"
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

ContextualSearchSessionHandle::TabValidator*
ContextualSearchSessionHandle::GetTabValidator() const {
  return service_ ? service_->GetTabValidator() : nullptr;
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

void ContextualSearchSessionHandle::SetIsBackgrounded(bool backgrounded) {
  // TODO(crbug.com/496926563): Add UMA logging for backgrounding to the
  // metrics recorder.
  if (auto* controller = GetController()) {
    controller->SetIsBackgrounded(backgrounded);
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
  bool mime_type_has_image = file_mime_type.find("image") != std::string::npos;

  if (lens::features::IsLensSendRawFileMediaTypesEnabled()) {
    // When the raw file media types feature is enabled, only set the mime type
    // to image if the file is an image, otherwise set it to unknown for all
    // other file types.
    if (mime_type_has_image && file_mime_type != "image/svg+xml") {
      mime_type = lens::MimeType::kImage;
    } else {
      mime_type = lens::MimeType::kUnknown;
    }
  } else {
    if (file_mime_type.find("pdf") != std::string::npos) {
      mime_type = lens::MimeType::kPdf;
    } else if (mime_type_has_image) {
      mime_type = lens::MimeType::kImage;
    } else {
      mime_type = lens::MimeType::kUnknown;
    }
  }

  std::unique_ptr<lens::ContextualInputData> input_data =
      std::make_unique<lens::ContextualInputData>();
  input_data->context_input = std::vector<lens::ContextualInput>();
  input_data->primary_content_type = mime_type;
  input_data->upload_type = lens::LensOverlayContextualInputUploadType::
      CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT;
  input_data->file_name = file_name;
  // For manual file uploads, the file name is also set in the page_title field.
  input_data->page_title = file_name;

  base::span<const uint8_t> file_data_span = base::span(file_bytes);
  std::vector<uint8_t> file_data_vector(file_data_span.begin(),
                                        file_data_span.end());
  input_data->context_input->push_back(
      lens::ContextualInput(std::move(file_data_vector), mime_type));
  input_data->mime_type_string = file_mime_type;

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

  if (contextual_input_data &&
      contextual_input_data->tab_session_id.has_value()) {
    deselected_tabs_urls_.erase(contextual_input_data->tab_session_id.value());
  }

  if (auto* metrics_recorder = GetMetricsRecorder()) {
    auto mime_type = contextual_input_data->primary_content_type.value_or(
        lens::MimeType::kUnknown);
    size_t page_contents_size = 0;

    if (contextual_input_data->context_input.has_value()) {
      for (const auto& input : *contextual_input_data->context_input) {
        page_contents_size += input.bytes_.size();
      }
    }

    size_t viewport_screenshot_size = 0;

    if (contextual_input_data->viewport_screenshot_bytes.has_value()) {
      viewport_screenshot_size +=
          contextual_input_data->viewport_screenshot_bytes->size();
    }

    if (contextual_input_data->viewport_screenshot.has_value()) {
      viewport_screenshot_size +=
          contextual_input_data->viewport_screenshot->computeByteSize();
    }

    size_t content_size = page_contents_size + viewport_screenshot_size;

    metrics_recorder->RecordFileSizeMetric(mime_type, content_size);
    metrics_recorder->RecordTabPartsSizes(viewport_screenshot_size,
                                          page_contents_size);
  }

  if (auto* controller = GetController()) {
    if (!contextual_input_data->upload_type.has_value()) {
      // If the input data did not already have an upload type (e.g.
      // auto-context) then it was the result of an explicit user upload.
      contextual_input_data->upload_type =
          lens::LensOverlayContextualInputUploadType::
              CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT;
    }
    controller->StartFileUploadFlow(
        file_token, std::move(contextual_input_data), image_options);
  }
}

void ContextualSearchSessionHandle::StartUrlContextUploadFlow(
    const base::UnguessableToken& file_token,
    const std::string& url) {
  // Exit early if the file token is not in the list of uploaded context
  // tokens, i.e. it was deleted before the upload flow could start.
  auto it = std::find(uploaded_context_tokens_.begin(),
                      uploaded_context_tokens_.end(), file_token);
  if (it == uploaded_context_tokens_.end()) {
    return;
  }

  if (auto* context_controller = GetController()) {
    auto contextual_input_data = std::make_unique<lens::ContextualInputData>();
    contextual_input_data->primary_content_type = lens::MimeType::kUnknown;
    contextual_input_data->parsed_url = url;
    context_controller->StartFileUploadFlow(
        file_token, std::move(contextual_input_data), std::nullopt);
  }
}

void ContextualSearchSessionHandle::StartDriveContextUploadFlow(
    const base::UnguessableToken& file_token,
    const DriveUploadParams& params) {
  // Exit early if the file token is not in the list of uploaded context
  // tokens, i.e. it was deleted before the upload flow could start.
  auto it = std::find(uploaded_context_tokens_.begin(),
                      uploaded_context_tokens_.end(), file_token);
  if (it == uploaded_context_tokens_.end()) {
    return;
  }

  if (auto* context_controller = GetController()) {
    auto contextual_input_data = std::make_unique<lens::ContextualInputData>();
    contextual_input_data->drive_id = params.drive_id;
    contextual_input_data->resource_key = params.resource_key;
    contextual_input_data->mime_type_string = params.mime_type;
    contextual_input_data->file_name = params.file_name;
    contextual_input_data->page_title = params.file_name;
    contextual_input_data->primary_content_type = lens::MimeType::kUnknown;
    contextual_input_data->upload_type =
        lens::LensOverlayContextualInputUploadType::
            CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT;
    context_controller->StartFileUploadFlow(
        file_token, std::move(contextual_input_data), std::nullopt);
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
  auto* context_controller = GetController();
  if (!context_controller) {
    return false;
  }

  const auto* file_info = context_controller->GetFileInfo(file_token);
  if (file_info == nullptr) {
    return false;
  }

  // Only support deselection for tabs when the feature is enabled.
  // Other file types (like images) cannot be deselected once submitted.
  bool is_tab = IsTabToken(file_token);
  bool is_tab_and_deselection_enabled =
      omnibox::IsTabDeselectionInComposeboxEnabled() && is_tab;

  bool is_submitted = std::find(submitted_context_tokens_.begin(),
                                submitted_context_tokens_.end(),
                                file_token) != submitted_context_tokens_.end();

  if (is_submitted && !is_tab_and_deselection_enabled) {
    // If the file was already submitted and deselection is not supported for
    // it, do not delete it.
    return false;
  }

  // Remove the file token from the list of uploaded context tokens.
  auto it = std::find(uploaded_context_tokens_.begin(),
                      uploaded_context_tokens_.end(), file_token);
  if (it != uploaded_context_tokens_.end()) {
    uploaded_context_tokens_.erase(it);
  }

  bool should_delete_from_controller = true;

  // Can delete tab fully (from controller) if it is NOT already submitted.
  // Otherwise, leave in controller for metadata.
  if (is_tab_and_deselection_enabled) {
    // Track that this tab was explicitly deselected in this session.
    deselected_tabs_urls_[file_info->tab_session_id.value()] = std::make_pair(
        file_info->tab_url.value_or(GURL()), file_info->tab_title.value_or(""));
    if (is_submitted) {
      // Remove the deselected tab from `submitted_context_tokens_`
      // so it is immediately excluded from the active query context and
      // tabstrip underlines. Do NOT delete it from the context controller
      // yet, as previous turns (e.g. `previous_turns_` tracking) still
      // reference this token to render their attachment metadata.
      std::erase(submitted_context_tokens_, file_token);
      should_delete_from_controller = false;
    }
  }

  lens::MimeType file_type = file_info->mime_type;
  contextual_search::ContextUploadStatus file_status = file_info->upload_status;

  bool success = true;
  if (should_delete_from_controller && context_controller) {
    success = context_controller->DeleteFile(file_token);
  }

  if (auto* metrics_recorder = GetMetricsRecorder()) {
    metrics_recorder->RecordFileDeletedMetrics(success, file_type, file_status);
  }

  // Clean up associated stale tokens with this tab. Do not erase
  // `persisted_tabs` since that has the `request_id` required to send a
  // deletion request to the server.
  if (success && is_tab_and_deselection_enabled) {
    SessionID session_id = file_info->tab_session_id.value();
    // Avoid duplicates to avoid deletion from the controller (on the second
    // delete, since after the first delete, the tab is no longer submitted and
    // thus deletable from controller). Tokens are few, so use flat set for
    // memory contiguousness and better cache locality.
    base::flat_set<base::UnguessableToken> other_tokens;
    for (const auto& token : uploaded_context_tokens_) {
      const auto* info = context_controller->GetFileInfo(token);
      if (info && info->tab_session_id == session_id && token != file_token) {
        other_tokens.insert(token);
      }
    }
    for (const auto& token : submitted_context_tokens_) {
      const auto* info = context_controller->GetFileInfo(token);
      if (info && info->tab_session_id == session_id && token != file_token) {
        other_tokens.insert(token);
      }
    }
    for (const auto& token : other_tokens) {
      DeleteFile(token);
    }
  }

  return success;
}

void ContextualSearchSessionHandle::ClearFiles(bool query_submitted) {
  if (query_submitted) {
    // When submitting query, always track tab tokens in `persisted_tabs_`
    // before clearing them from `uploaded_context_tokens_`.
    for (const auto& token : uploaded_context_tokens_) {
      MaybeAddTabToPersistedTabs(token);
    }
  }
  // `uploaded_context_tokens_` is always cleared upon query submission or
  // cancel since they are only for 1 round of submissions:
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
  NotifyQuerySubmittedSessionState(uploaded_file_infos,
                                   search_url_request_info->query_text.size());
  metrics_recorder->NotifySessionStateChanged(
      contextual_search::SessionState::kNavigationOccurred);

  // If the request info has no file tokens, move the uploaded tokens to the
  // request. Otherwise, keep the file tokens as is and remove them from the
  // uploaded context tokens manually. Treat tabs the same way.
  if (search_url_request_info->file_tokens.empty()) {
    search_url_request_info->file_tokens =
        std::exchange(uploaded_context_tokens_, {});
  } else {
    // For lens queries, handle subset of files chosen:
    for (const auto& token : search_url_request_info->file_tokens) {
      std::erase(uploaded_context_tokens_, token);
    }
  }

  // Copy the tokens from this request to the list of all submitted tokens.
  submitted_context_tokens_.insert(submitted_context_tokens_.end(),
                                   search_url_request_info->file_tokens.begin(),
                                   search_url_request_info->file_tokens.end());

  // Track submitted tabs for the next turn.
  for (const auto& token : search_url_request_info->file_tokens) {
    MaybeAddTabToPersistedTabs(token);
  }

  // Set the invocation source on the search URL request info, if it is not
  // already set.
  if (!search_url_request_info->invocation_source.has_value()) {
    search_url_request_info->invocation_source = invocation_source_;
  }

  context_controller->CreateSearchUrl(std::move(search_url_request_info),
                                      std::move(callback));
}

void ContextualSearchSessionHandle::set_smart_tab_sharing_active(
    std::optional<bool> active) {
  if (smart_tab_sharing_active_.value_or(false) != active.value_or(false)) {
    smart_tab_sharing_toggled_since_last_turn_ = true;
  }
  smart_tab_sharing_active_ = active;
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

  auto* tab_validator = GetTabValidator();

  if (smart_tab_sharing_toggled_since_last_turn_) {
    std::vector<lens::LensOverlayRequestId> expired_contexts;

    // Collect request IDs from submitted tabs.
    for (const auto& [session_id, token_and_req] : persisted_tabs_) {
      expired_contexts.push_back(token_and_req.second);
    }

    // Collect request IDs from uploaded context tokens.
    for (const auto& token : uploaded_context_tokens_) {
      const auto* file_info = context_controller->GetFileInfo(token);
      if (file_info && file_info->request_id.has_value()) {
        expired_contexts.push_back(file_info->request_id.value());
      }
    }

    // Collect request IDs from submitted context tokens.
    for (const auto& token : submitted_context_tokens_) {
      const auto* file_info = context_controller->GetFileInfo(token);
      if (file_info && file_info->request_id.has_value()) {
        expired_contexts.push_back(file_info->request_id.value());
      }
    }

    for (const auto& req_id : expired_contexts) {
      bool already_present = false;
      std::string req_id_str = req_id.SerializeAsString();
      for (const auto& existing :
           create_client_to_aim_request_info->removed_contexts) {
        if (existing.SerializeAsString() == req_id_str) {
          already_present = true;
          break;
        }
      }
      if (!already_present) {
        create_client_to_aim_request_info->removed_contexts.push_back(req_id);
      }
    }

    persisted_tabs_.clear();
    uploaded_context_tokens_.clear();
    submitted_context_tokens_.clear();
    smart_tab_sharing_toggled_since_last_turn_ = false;
  }

  // Check for closed/navigated/removed tabs.
  std::vector<SessionID> deleted_tabs;
  bool context_management_enabled =
      base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox);
  bool signal_browser_tab_deletions = base::FeatureList::IsEnabled(
      lens::features::kLensDeleteContextOnPageNavigation);

  for (const auto& [session_id, token_and_req] : persisted_tabs_) {
    base::UnguessableToken token_to_validate;

    if (context_management_enabled) {
      // If explicitly deselected by user, treat as deleted. Check this
      // directly because committed tabs are cleared from active lists.
      if (deselected_tabs_urls_.contains(session_id)) {
        deleted_tabs.push_back(session_id);
        continue;
      }

      token_to_validate = GetActiveTokenForTab(session_id);
      if (token_to_validate.is_empty()) {
        // If not active, it might be committed. Fallback to the original token.
        token_to_validate = token_and_req.first;
      }

      // If the token does not match the last submitted token, then that means
      // the webpage in the tab has changed and recontextualization has
      // triggered, (meaning the tab is still attached; its contents have been
      // updated). Thus, remove that old stale token without notifying the
      // server. Do not notify the server since the tab is still attached, it
      // just has new content (and a new token). This can trigger, but the
      // closed/deleted tab logic down below can also trigger since they
      // are not mutually exclusive.
      if (token_to_validate != token_and_req.first) {
        // Delete from `uploaded_context_tokens_` as a safety deletion.
        // Recontextualization only happens to submitted tabs, so any
        // potentially stale `uploaded_context_tokens` should have been cleared
        // last query right after submission, but before recontextualization.
        std::erase(uploaded_context_tokens_, token_and_req.first);
        std::erase(submitted_context_tokens_, token_and_req.first);
      }
    } else {
      // Flag disabled: `uploaded_context_tokens_` is cleared after each query.
      // Validate the stored token directly against the browser.
      token_to_validate = token_and_req.first;
    }

    if (signal_browser_tab_deletions && tab_validator &&
        !token_to_validate.is_empty()) {
      const auto* file_info =
          context_controller->GetFileInfo(token_to_validate);
      // If the tab is closed, or the URL is no longer being tracked after
      // navigation (no recontextualization).
      if (file_info && !tab_validator->IsTabValidAndPointingToUrl(*file_info)) {
        deleted_tabs.push_back(session_id);
      }
    }
  }

  // Remove fully navigated tabs that are no longer tracked since there was no
  // recontextualization to track them. Also remove closed tabs. Notify server
  // of any of these tracking removals.
  for (const auto& session_id : deleted_tabs) {
    auto it = persisted_tabs_.find(session_id);
    if (it != persisted_tabs_.end()) {
      create_client_to_aim_request_info->removed_contexts.push_back(
          it->second.second);

      // If the tab is closed, we remove all tokens associated with it.
      // If the tab is still open (navigated), we only remove the superceded
      // token that failed validation (stored_token).
      bool tab_still_open = false;
      if (tab_validator) {
        // If there is an active (potentially new) token for this tab, check if
        // it is valid. If it is valid, the tab is still open (navigated).
        base::UnguessableToken active_token = GetActiveTokenForTab(session_id);
        base::UnguessableToken validated_token =
            context_management_enabled ? active_token : it->second.first;
        // If context management is enabled, always validate the active token
        // if present (it might be a deselected tab skipped during validation
        // earlier). If disabled, only validate if it is a new token
        // (navigated).
        if (!active_token.is_empty() &&
            (context_management_enabled || active_token != validated_token)) {
          const auto* active_file_info =
              context_controller->GetFileInfo(active_token);
          if (active_file_info &&
              tab_validator->IsTabValidAndPointingToUrl(*active_file_info)) {
            tab_still_open = true;
          }
        }
      }

      if (!tab_still_open) {
        // If closed, remove all tab tokens associated with this tab session id.
        // `isTabToken()` is not needed here to verify contexts are a tab
        // because successfully matching session id's with a valid `session_id`
        // means that the context must be a tab.
        std::erase_if(uploaded_context_tokens_, [&](const auto& token) {
          const auto* file_info = context_controller->GetFileInfo(token);
          return file_info && file_info->tab_session_id == session_id;
        });
        std::erase_if(submitted_context_tokens_, [&](const auto& token) {
          const auto* file_info = context_controller->GetFileInfo(token);
          return file_info && file_info->tab_session_id == session_id;
        });
      } else {
        // Remove only the expired token that navigation made irrelevant.
        std::erase(uploaded_context_tokens_, it->second.first);
        std::erase(submitted_context_tokens_, it->second.first);
      }

      persisted_tabs_.erase(it);
    }
  }

  // Move the uploaded tokens to the request's file_tokens. Make sure to dedupe
  // the tokens with those already in the ClientToAimRequestInfo.
  base::flat_set<base::UnguessableToken> file_tokens_set(
      std::move(create_client_to_aim_request_info->file_tokens));
  // Deduplicate file tokens by adding tokens to set that is sent in
  // this current request/query submission.
  file_tokens_set.insert(uploaded_context_tokens_.begin(),
                         uploaded_context_tokens_.end());
  // Keep tabs but clear the files. Move any tab tokens in current
  // turn/submission into `persisted_tabs_`.
  ClearFiles(/*query_submitted=*/true);
  create_client_to_aim_request_info->file_tokens =
      std::move(file_tokens_set).extract();

  // Copy the tokens from this request to the list of all submitted tokens.
  submitted_context_tokens_.insert(
      submitted_context_tokens_.end(),
      create_client_to_aim_request_info->file_tokens.begin(),
      create_client_to_aim_request_info->file_tokens.end());

  if (GetMetricsRecorder()) {
    NotifyQuerySubmittedSessionState(
        TokensToFileInfos(GetController(),
                          create_client_to_aim_request_info->file_tokens),
        create_client_to_aim_request_info->query_text.size());
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

std::vector<std::string>
ContextualSearchSessionHandle::GetSubmittedContextTabTitles() const {
  std::vector<std::string> titles;
  for (const auto& file_info : GetSubmittedContextFileInfos()) {
    if (file_info.tab_title.has_value()) {
      titles.push_back(file_info.tab_title.value());
    }
  }
  return titles;
}

void ContextualSearchSessionHandle::ClearSubmittedContextTokens() {
  submitted_context_tokens_.clear();
}

void ContextualSearchSessionHandle::set_submitted_context_tokens(
    const std::vector<base::UnguessableToken>& tokens) {
  submitted_context_tokens_ = tokens;
}

void ContextualSearchSessionHandle::set_persisted_tabs(
    PersistedTabsMap persisted_tabs) {
  persisted_tabs_ = std::move(persisted_tabs);
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

bool ContextualSearchSessionHandle::IsTabToken(
    const base::UnguessableToken& token) const {
  auto* controller = GetController();
  if (!controller) {
    return false;
  }
  const auto* file_info = controller->GetFileInfo(token);
  return file_info && file_info->tab_session_id.has_value() &&
         file_info->tab_session_id->is_valid();
}

base::UnguessableToken ContextualSearchSessionHandle::GetActiveTokenForTab(
    SessionID tab_session_id) const {
  auto* context_controller = GetController();
  if (!context_controller) {
    return base::UnguessableToken();
  }
  for (const auto& token : uploaded_context_tokens_) {
    if (IsTabToken(token)) {
      const auto* file_info = context_controller->GetFileInfo(token);
      if (file_info && file_info->tab_session_id == tab_session_id &&
          !file_info->is_superceded) {
        return token;
      }
    }
  }
  // TODO(crbug.com/528416084): Deduplicate these loops using a helper.
  for (const auto& token : submitted_context_tokens_) {
    if (IsTabToken(token)) {
      const auto* file_info = context_controller->GetFileInfo(token);
      if (file_info && file_info->tab_session_id == tab_session_id &&
          !file_info->is_superceded) {
        return token;
      }
    }
  }
  if (base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox)) {
    auto it = persisted_tabs_.find(tab_session_id);
    if (it != persisted_tabs_.end()) {
      return it->second.first;
    }
  }

  return base::UnguessableToken();
}

void ContextualSearchSessionHandle::MaybeAddTabToPersistedTabs(
    const base::UnguessableToken& token) {
  if (IsTabToken(token)) {
    auto* controller = GetController();
    if (controller) {
      const auto* file_info = controller->GetFileInfo(token);
      if (file_info && !file_info->is_superceded) {
        SessionID session_id = file_info->tab_session_id.value();
        // Request ID must exist, as the tab was already submitted to server.
        CHECK(file_info->request_id.has_value());
        lens::LensOverlayRequestId req_id = file_info->request_id.value();
        persisted_tabs_[session_id] = std::make_pair(token, req_id);
      }
    }
  }
}

void ContextualSearchSessionHandle::NotifyQuerySubmittedSessionState(
    const std::vector<FileInfo>& file_infos,
    int query_text_length) {
  if (auto* metrics_recorder = GetMetricsRecorder()) {
    bool has_tab_context = false;
    bool has_non_tab_context = false;
    bool has_drive_context = false;
    int tab_count = 0;
    for (const auto& file_info : file_infos) {
      if (file_info.tab_url.has_value()) {
        has_tab_context = true;
      } else {
        has_non_tab_context = true;
      }
      if (file_info.input_data && file_info.input_data->drive_id.has_value()) {
        has_drive_context = true;
      }
      if (file_info.mime_type == lens::MimeType::kAnnotatedPageContent) {
        tab_count++;
      }
    }
    metrics_recorder->NotifyQuerySubmitted(has_tab_context, has_non_tab_context,
                                           query_text_length, file_infos.size(),
                                           has_drive_context);
    if (tab_count > 0) {
      metrics_recorder->RecordAttachmentCountAtSubmission(
          lens::MimeType::kAnnotatedPageContent, tab_count);
    }
  }
}

void ContextualSearchSessionHandle::AddThreadTurn(
    const contextual_tasks::ThreadTurn& turn) {
  previous_turns_.push_back(turn);
}

base::UnguessableToken ContextualSearchSessionHandle::GetTokenForTab(
    SessionID tab_session_id) const {
  base::UnguessableToken active_token = GetActiveTokenForTab(tab_session_id);
  if (!active_token.is_empty()) {
    return active_token;
  }
  auto it = persisted_tabs_.find(tab_session_id);
  if (it != persisted_tabs_.end()) {
    return it->second.first;
  }
  return base::UnguessableToken();
}

bool ContextualSearchSessionHandle::IsTabDeselected(
    SessionID tab_session_id,
    const GURL& current_url,
    const std::string& current_title) const {
  auto it = deselected_tabs_urls_.find(tab_session_id);
  if (it == deselected_tabs_urls_.end()) {
    return false;
  }

  bool is_equivalent = false;
  if (auto* validator = GetTabValidator()) {
    is_equivalent = validator->AreUrlsEquivalent(
        it->second.first, it->second.second, current_url, current_title);
  } else {
    is_equivalent =
        it->second.first.GetWithoutRef() == current_url.GetWithoutRef();
  }

  if (!is_equivalent) {
    deselected_tabs_urls_.erase(it);
    return false;
  }
  return true;
}

bool ContextualSearchSessionHandle::AreUrlsEquivalent(
    const GURL& url1,
    const std::string& title1,
    const GURL& url2,
    const std::string& title2) const {
  if (auto* validator = GetTabValidator()) {
    return validator->AreUrlsEquivalent(url1, title1, url2, title2);
  }
  return url1.GetWithoutRef() == url2.GetWithoutRef();
}

void ContextualSearchSessionHandle::RemoveDeselectedTab(
    SessionID tab_session_id) {
  deselected_tabs_urls_.erase(tab_session_id);
}

}  // namespace contextual_search
