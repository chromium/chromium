// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/contextual_search_session_handle.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/unguessable_token.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_tasks/public/account_utils.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/query_contextualizer.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_features.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "contextual_search_context_controller.h"
#include "contextual_search_types.h"
#include "pref_names.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "third_party/lens_server_proto/modality_chip_props.pb.h"

namespace contextual_search {
bool TabInfo::operator==(const TabInfo& other) const {
  if (context_token != other.context_token || tab_id != other.tab_id ||
      url != other.url || title != other.title || uploaded != other.uploaded ||
      restored_from_aim != other.restored_from_aim ||
      submitted != other.submitted ||
      pending_delayed_upload != other.pending_delayed_upload ||
      request_id.has_value() != other.request_id.has_value()) {
    return false;
  }
  if (request_id.has_value() && request_id->SerializeAsString() !=
                                    other.request_id->SerializeAsString()) {
    return false;
  }
  return true;
}

bool TabContextState::operator==(const TabContextState& other) const {
  return attached == other.attached && restored == other.restored &&
         deselected == other.deselected;
}

bool TabContextState::ContainsTab(SessionID tab_id) const {
  if (!tab_id.is_valid()) {
    return false;
  }
  const auto has_tab_id = [tab_id](const TabInfo& tab) {
    return tab.tab_id == tab_id.id();
  };
  return std::ranges::any_of(attached, has_tab_id) ||
         std::ranges::any_of(restored, has_tab_id);
}

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

// Marks the file associated with `token` as superceded and returns a mutable
// pointer to its FileInfo. Returns nullptr if `controller` is null or if no
// file is associated with `token`.
// TODO(crbug.com/564475334): See if this can be moved to
// `ContextualSearchContextController`.
FileInfo* MarkFileSuperceded(ContextualSearchContextController* controller,
                             const base::UnguessableToken& token) {
  if (!controller) {
    return nullptr;
  }
  const auto* file_info = controller->GetFileInfo(token);
  if (!file_info) {
    return nullptr;
  }
  auto* mutable_file_info = const_cast<FileInfo*>(file_info);
  mutable_file_info->is_superceded = true;
  return mutable_file_info;
}

// Appends `request_id` to `request_ids` unless an equal request ID is already
// present. Returns true if `request_id` was appended.
bool AppendUniqueRequestId(std::vector<lens::LensOverlayRequestId>& request_ids,
                           const lens::LensOverlayRequestId& request_id) {
  const std::string serialized = request_id.SerializeAsString();
  for (const auto& existing : request_ids) {
    if (existing.SerializeAsString() == serialized) {
      return false;
    }
  }
  request_ids.push_back(request_id);
  return true;
}

void LogRemoveContextReason(ContextualTasksRemovedContextReason reason) {
  base::UmaHistogramEnumeration("ContextualTasks.RemovedContext.Reason",
                                reason);
}

// Returns whether the web identity for this session matches the browser
// profile's primary account.
// - Pre-rearchitecture (and in the Omnibox/NTP), `auth_user_index` is 0 and
//   `contextual_tasks::IsUrlForPrimaryAccount` checks the user index from
//   `current_url` (defaulting to index 0 in the cookie jar when `current_url`
//   has no user index, such as on `chrome://new-tab-page`).
// - Post-rearchitecture (`kContextualTasksRearchitecture`), `current_url` is
//   the URL of the page the Contextual Tasks extension is embedded into (i.e.
//   `www.google.com/search`) and `auth_user_index` is populated from the
//   handshake response (used when `current_url` does not specify a user index).
bool IsSessionForPrimaryAccount(signin::IdentityManager* identity_manager,
                                const GURL& current_url,
                                size_t auth_user_index) {
  if (!identity_manager) {
    return false;
  }
  if (contextual_tasks::GetUserIndex(current_url).has_value() ||
      auth_user_index == 0) {
    return contextual_tasks::IsUrlForPrimaryAccount(identity_manager,
                                                    current_url);
  }
  CoreAccountInfo primary_account =
      contextual_tasks::GetPrimaryAccountInfoFromProfile(identity_manager);
  if (primary_account.IsEmpty()) {
    return false;
  }
  auto accounts_in_cookie_jar = identity_manager->GetAccountsInCookieJar();
  const std::vector<gaia::ListedAccount>& accounts =
      accounts_in_cookie_jar.GetAllAccounts();
  if (auth_user_index >= accounts.size()) {
    return false;
  }
  return primary_account.gaia == accounts[auth_user_index].gaia_id;
}

// Returns whether `token` is present in `list`. If `must_be_submitted` is true,
// the entry must also have `submitted == true`.
bool IsTokenInTabList(const std::vector<TabInfo>& list,
                      const base::UnguessableToken& token,
                      bool must_be_submitted) {
  return std::ranges::any_of(list, [&](const TabInfo& tab) {
    return tab.context_token == token && (!must_be_submitted || tab.submitted);
  });
}

// Returns whether explicit tab deselection is enabled for composebox or
// Contextual Tasks rearchitecture sessions.
bool IsTabDeselectionEnabled() {
  return omnibox::IsTabDeselectionInComposeboxEnabled() ||
         contextual_tasks::IsContextualTasksRearchitectureEnabled() ||
         contextual_tasks::IsContextualTasksSidePanelRearchitectureEnabled();
}

bool IsImplicitInitialQueryUpload(const FileInfo& info) {
  return info.input_data &&
         (info.is_implicit_upload || info.input_data->is_implicit_upload) &&
         info.input_data->upload_type ==
             lens::LensOverlayContextualInputUploadType::
                 CONTEXTUAL_INPUT_UPLOAD_TYPE_CONTEXTUAL_SEARCHBOX_INITIAL_QUERY;
}

bool IsPdfFileInfo(const FileInfo& info) {
  if (IsImplicitInitialQueryUpload(info)) {
    return false;
  }
  if (info.mime_type == lens::MimeType::kPdf) {
    return true;
  }
  if (info.mime_type_string.has_value() &&
      info.mime_type_string->find("pdf") != std::string::npos) {
    return true;
  }
  return info.input_data && info.input_data->mime_type_string.has_value() &&
         info.input_data->mime_type_string->find("pdf") != std::string::npos;
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
  if (suggest_inputs->has_encoded_request_id() ||
      suggest_inputs->has_encoded_contextual_inputs()) {
    return *suggest_inputs.get();
  }

  return std::nullopt;
}

std::string ContextualSearchSessionHandle::search_session_id() const {
  auto* controller = GetController();
  return controller ? controller->search_session_id() : "";
}

std::optional<lens::LensOverlayVisualSearchInteractionData>
ContextualSearchSessionHandle::GetVisualSearchInteractionData(
    const base::UnguessableToken& file_token,
    const std::optional<std::string>& query_text) {
  auto* controller = GetController();
  return controller ? controller->GetVisualSearchInteractionData(file_token,
                                                                 query_text)
                    : std::nullopt;
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

  if (!contextual_input_data) {
    uploaded_context_tokens_.erase(it);
    return;
  }

  if (service_) {
    base::UmaHistogramBoolean(
        "ContextualSearch.TabContextAdded.AccountMatches",
        IsSessionForPrimaryAccount(service_->identity_manager_, current_url_,
                                   auth_user_index_));
  }

  if (contextual_input_data->tab_session_id.has_value()) {
    tab_context_.deselected.erase(
        contextual_input_data->tab_session_id.value());
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
    if (contextual_input_data->tab_session_id.has_value()) {
      TabInfo tab;
      tab.context_token = file_token;
      tab.tab_id = contextual_input_data->tab_session_id->id();
      tab.url = contextual_input_data->page_url.value_or(GURL());
      tab.title = contextual_input_data->page_title.value_or("");
      tab.uploaded = !contextual_input_data->is_implicit_upload;
      // Updates in place if the tab is already attached (e.g. it was
      // attached before upload, or is being re-uploaded with a new token).
      AddAttachedTab(std::move(tab));
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
    // A delayed tab has not been uploaded yet, so it has no file info in the
    // controller, but it may still be attached.
    delayed_tab_token_ids_.erase(file_token);
    RemoveAttachedTabForToken(file_token);
    return false;
  }

  // Only support deselection for tabs when the feature is enabled.
  // Other file types (like images) cannot be deselected once submitted.
  bool is_tab = IsTabToken(file_token);
  bool is_tab_and_deselection_enabled = IsTabDeselectionEnabled() && is_tab;

  // Check whether `file_token` was already sent to the server in a previous
  // query (either still in `submitted_context_tokens_`, or persisted as a
  // submitted tab in `tab_context_.attached` or `displaced_submitted_tabs_`
  // after `ClearSubmittedContextTokens()` ran).
  bool was_sent_to_server =
      std::ranges::contains(submitted_context_tokens_, file_token) ||
      IsTokenInTabList(tab_context_.attached, file_token,
                       /*must_be_submitted=*/true) ||
      IsTokenInTabList(displaced_submitted_tabs_, file_token,
                       /*must_be_submitted=*/false);

  if (was_sent_to_server && !is_tab_and_deselection_enabled) {
    // If the file was already sent to the server and deselection is not
    // supported for it, do not delete it.
    return false;
  }

  // Remove the file token from the list of uploaded context tokens.
  auto it = std::find(uploaded_context_tokens_.begin(),
                      uploaded_context_tokens_.end(), file_token);
  if (it != uploaded_context_tokens_.end()) {
    uploaded_context_tokens_.erase(it);
  }

  bool should_delete_from_controller = true;

  // Can delete tab fully (from controller) if it was NOT already sent to the
  // server. Otherwise, leave in controller for metadata.
  auto* tab_validator = GetTabValidator();
  const bool is_current_tab_valid =
      !tab_validator || tab_validator->IsTabValidAndPointingToUrl(*file_info);
  if (is_tab_and_deselection_enabled) {
    if (is_current_tab_valid) {
      // Track that this tab was explicitly deselected in this session.
      tab_context_.deselected[file_info->tab_session_id.value()] =
          std::make_pair(file_info->tab_url.value_or(GURL()),
                         file_info->tab_title.value_or(""));
    }
    if (was_sent_to_server) {
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
  std::optional<SessionID> tab_session_id = file_info->tab_session_id;

  bool success = true;
  if (should_delete_from_controller && context_controller) {
    success = context_controller->DeleteFile(file_token);
  }

  if (auto* metrics_recorder = GetMetricsRecorder()) {
    metrics_recorder->RecordFileDeletedMetrics(success, file_type, file_status);
  }

  // Remove the attached tab if its current token was deleted. If the entry was
  // already submitted, `RemoveAttachedTabForToken()` moves it into
  // `displaced_submitted_tabs_` so `TakeRemovedContexts()` can report its
  // `request_id` on the next query submission.
  if (success) {
    RemoveAttachedTabForToken(file_token);
  }

  // Clean up associated stale tokens with this tab.
  if (success && is_tab_and_deselection_enabled && tab_session_id.has_value()) {
    SessionID session_id = tab_session_id.value();
    // Avoid duplicates to avoid deletion from the controller (on the second
    // delete, since after the first delete, the tab is no longer submitted and
    // thus deletable from controller). Tokens are few, so use flat set for
    // memory contiguousness and better cache locality.
    base::flat_set<base::UnguessableToken> other_tokens;
    if (context_controller) {
      auto maybe_add_other_token = [&](const base::UnguessableToken& token) {
        if (token == file_token) {
          return;
        }
        const auto* info = context_controller->GetFileInfo(token);
        if (info && info->tab_session_id == session_id) {
          // If `file_token` was removed because it is no longer valid (e.g.
          // the tab navigated to a new URL), do not delete a newly uploaded
          // token that is valid for the tab's current URL.
          if (!is_current_tab_valid && tab_validator &&
              tab_validator->IsTabValidAndPointingToUrl(*info)) {
            return;
          }
          other_tokens.insert(token);
        }
      };
      for (const auto& token : uploaded_context_tokens_) {
        maybe_add_other_token(token);
      }
      for (const auto& token : submitted_context_tokens_) {
        maybe_add_other_token(token);
      }
      for (const TabInfo& tab : tab_context_.attached) {
        if (tab.tab_id == session_id.id()) {
          maybe_add_other_token(tab.context_token);
        }
      }
    }
    for (const auto& token : other_tokens) {
      DeleteFile(token);
    }
  }

  return success;
}

bool ContextualSearchSessionHandle::RemoveUploadedContextToken(
    const base::UnguessableToken& file_token) {
  RemoveAttachedTabForToken(file_token);
  return std::erase(uploaded_context_tokens_, file_token) > 0;
}

void ContextualSearchSessionHandle::ClearFiles(bool query_submitted) {
  if (query_submitted) {
    // When submitting query, always track tab tokens as 'persisted tabs'
    // in `tab_context_.attached` before clearing them from
    // `uploaded_context_tokens_`.
    for (const auto& token : uploaded_context_tokens_) {
      MarkTabAckByServer(token);
    }
  } else {
    // On cancel, drop attached tabs that were never submitted: tabs whose
    // token is still pending and delayed tabs pending upload. Submitted tabs
    // stay attached.
    const base::flat_set<base::UnguessableToken> pending(
        uploaded_context_tokens_.begin(), uploaded_context_tokens_.end());

    if (std::erase_if(tab_context_.attached, [&pending](const TabInfo& tab) {
          return tab.pending_delayed_upload ||
                 pending.contains(tab.context_token);
        }) > 0) {
      NotifyTabContextSubscribers();
    }
  }
  // `uploaded_context_tokens_` is always cleared upon query submission or
  // cancel since they are only for 1 round of submissions:
  uploaded_context_tokens_.clear();
  MaybeNotifyTabContextSubscribers();
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

  if (!search_url_request_info->file_tokens.empty()) {
    has_submitted_context_ = true;
    if (std::ranges::any_of(
            TokensToFileInfos(context_controller,
                              search_url_request_info->file_tokens),
            IsPdfFileInfo)) {
      has_submitted_pdf_context_ = true;
    }
  }

  // Copy the tokens from this request to the list of all submitted tokens.
  submitted_context_tokens_.insert(submitted_context_tokens_.end(),
                                   search_url_request_info->file_tokens.begin(),
                                   search_url_request_info->file_tokens.end());

  // Track submitted tabs for the next turn.
  for (const auto& token : search_url_request_info->file_tokens) {
    MarkTabAckByServer(token);
  }

  displaced_submitted_tabs_.clear();
  sts_toggled_removed_contexts_.clear();
  smart_tab_sharing_toggled_since_last_turn_ = false;

  // Set the invocation source on the search URL request info, if it is not
  // already set.
  if (!search_url_request_info->invocation_source.has_value()) {
    search_url_request_info->invocation_source = invocation_source_;
  }

  MaybeNotifyTabContextSubscribers();
  context_controller->CreateSearchUrl(std::move(search_url_request_info),
                                      std::move(callback));
}

void ContextualSearchSessionHandle::OnSmartTabSharingToggled(bool active) {
  if (smart_tab_sharing_active_.value_or(!active) != active) {
    auto* context_controller = GetController();

    if (!active) {
      smart_tab_sharing_toggled_since_last_turn_ = true;

      auto add_removed_context = [&](const base::UnguessableToken& token) {
        if (auto* file_info = MarkFileSuperceded(context_controller, token)) {
          if (file_info->request_id.has_value()) {
            sts_toggled_removed_contexts_.push_back(
                file_info->request_id.value());
          }
          if (file_info->tab_session_id.has_value()) {
            tab_context_.deselected[file_info->tab_session_id.value()] =
                std::make_pair(file_info->tab_url.value_or(GURL()),
                               file_info->tab_title.value_or(""));
          }
        }
      };

      // Collect request IDs and add to `tab_context_.deselected` so
      // GetTabsFromContext in ActiveTaskContextProviderImpl filters out
      // submitted task attachments.
      for (const TabInfo& tab : tab_context_.attached) {
        if (!tab.submitted) {
          continue;
        }
        sts_toggled_removed_contexts_.push_back(tab.request_id.value());
        if (auto* file_info =
                MarkFileSuperceded(context_controller, tab.context_token)) {
          if (file_info->tab_session_id.has_value()) {
            tab_context_.deselected[file_info->tab_session_id.value()] =
                std::make_pair(file_info->tab_url.value_or(GURL()),
                               file_info->tab_title.value_or(""));
          }
        }
      }
      for (const TabInfo& tab : displaced_submitted_tabs_) {
        if (tab.request_id.has_value()) {
          sts_toggled_removed_contexts_.push_back(tab.request_id.value());
        }
      }

      // Collect request IDs from uploaded context tokens.
      for (const auto& token : uploaded_context_tokens_) {
        add_removed_context(token);
      }

      // Collect request IDs from submitted context tokens.
      for (const auto& token : submitted_context_tokens_) {
        add_removed_context(token);
      }

      // Immediately clear submitted and uploaded context tokens and persisted
      // tabs so tab strip underlines and context state clear instantly in the
      // UI.
      ClearAllPersistedTabs();
      submitted_context_tokens_.clear();
      uploaded_context_tokens_.clear();
      has_submitted_pdf_context_ = false;
    } else {
      auto is_smart_or_implicit_tab = [&](const base::UnguessableToken& token) {
        if (!context_controller) {
          return false;
        }
        const auto* file_info = context_controller->GetFileInfo(token);
        if (!file_info) {
          return false;
        }
        return (file_info->input_data &&
                file_info->input_data->was_smart_tab_selection) ||
               file_info->is_implicit_upload;
      };

      auto maybe_remove_manual_context =
          [&](const base::UnguessableToken& token) {
            if (is_smart_or_implicit_tab(token)) {
              return false;
            }
            if (auto* file_info =
                    MarkFileSuperceded(context_controller, token)) {
              if (file_info->request_id.has_value()) {
                sts_toggled_removed_contexts_.push_back(
                    file_info->request_id.value());
              }
              if (file_info->tab_session_id.has_value()) {
                tab_context_.deselected[file_info->tab_session_id.value()] =
                    std::make_pair(file_info->tab_url.value_or(GURL()),
                                   file_info->tab_title.value_or(""));
              }
            }
            return true;
          };

      bool did_delete_tab =
          std::erase_if(tab_context_.attached, [&](const TabInfo& tab) {
            if (!tab.submitted || is_smart_or_implicit_tab(tab.context_token)) {
              return false;
            }
            sts_toggled_removed_contexts_.push_back(tab.request_id.value());
            if (auto* file_info =
                    MarkFileSuperceded(context_controller, tab.context_token)) {
              if (file_info->tab_session_id.has_value()) {
                tab_context_.deselected[file_info->tab_session_id.value()] =
                    std::make_pair(file_info->tab_url.value_or(GURL()),
                                   file_info->tab_title.value_or(""));
              }
            }
            return true;
          });
      if (did_delete_tab) {
        tab_context_dirty_ = true;
      }

      std::erase_if(uploaded_context_tokens_, maybe_remove_manual_context);
      std::erase_if(submitted_context_tokens_, maybe_remove_manual_context);

      if (!sts_toggled_removed_contexts_.empty()) {
        smart_tab_sharing_toggled_since_last_turn_ = true;
      }
    }

    // Deduplicate collected request IDs.
    std::vector<lens::LensOverlayRequestId> unique_reqs;
    for (const auto& req : sts_toggled_removed_contexts_) {
      std::string req_str = req.SerializeAsString();
      bool found = false;
      for (const auto& existing : unique_reqs) {
        if (existing.SerializeAsString() == req_str) {
          found = true;
          break;
        }
      }
      if (!found) {
        unique_reqs.push_back(req);
      }
    }
    sts_toggled_removed_contexts_ = std::move(unique_reqs);
  }
  smart_tab_sharing_active_ = active;
  MaybeNotifyTabContextSubscribers();
}

std::vector<lens::LensOverlayRequestId>
ContextualSearchSessionHandle::TakeRemovedContexts() {
  std::vector<lens::LensOverlayRequestId> removed_contexts;
  auto* context_controller = GetController();
  if (!context_controller) {
    return removed_contexts;
  }

  auto* tab_validator = GetTabValidator();

  if (smart_tab_sharing_toggled_since_last_turn_ ||
      !sts_toggled_removed_contexts_.empty()) {
    for (const auto& req_id : sts_toggled_removed_contexts_) {
      if (AppendUniqueRequestId(removed_contexts, req_id)) {
        LogRemoveContextReason(
            ContextualTasksRemovedContextReason::kSmartTabSharingDisabled);
      }
    }
    sts_toggled_removed_contexts_.clear();
    smart_tab_sharing_toggled_since_last_turn_ = false;
  }

  bool context_management_enabled =
      base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox);
  bool signal_browser_tab_deletions = base::FeatureList::IsEnabled(
      lens::features::kLensDeleteContextOnPageNavigation);

  auto maybe_remove_persisted_tab = [&](const TabInfo& tab, bool is_displaced) {
    if (!tab.submitted || !tab.tab_id.has_value()) {
      return false;
    }
    const SessionID session_id = SessionID::FromSerializedValue(*tab.tab_id);
    const base::UnguessableToken& persisted_token = tab.context_token;
    base::UnguessableToken token_to_validate;
    bool should_remove_from_server = false;

    // If explicitly deselected by user, treat as deleted. Check this directly
    // because committed tabs are cleared from active lists and desktop side
    // panel deselection does not require `kContextManagementInComposebox`.
    if (IsTabDeselectionEnabled() &&
        tab_context_.deselected.contains(session_id)) {
      should_remove_from_server = true;
    } else if (context_management_enabled) {
      token_to_validate = GetActiveTokenForTab(session_id);
      if (token_to_validate.is_empty()) {
        // If not active, it might be committed. Fallback to the original
        // token.
        token_to_validate = persisted_token;
      }

      // If the token does not match the last submitted token, a newer token
      // exists for the same `session_id`. If recontextualized on the same
      // page, `persisted_token` is superceded and no removal is sent. If the
      // tab navigated to a new URL (`!persisted_file_info->is_superceded` and
      // `!tab_validator->IsTabValidAndPointingToUrl(*persisted_file_info)`),
      // `persisted_token` is stale because it represents a previous page URL;
      // we set `token_to_validate = persisted_token` so validation fails
      // below and `persisted_token`'s `request_id` is sent in
      // `removed_contexts` while keeping the new token for the new URL.
      if (token_to_validate != persisted_token) {
        const auto* persisted_file_info =
            context_controller->GetFileInfo(persisted_token);
        if (signal_browser_tab_deletions && tab_validator &&
            persisted_file_info && !persisted_file_info->is_superceded &&
            !tab_validator->IsTabValidAndPointingToUrl(*persisted_file_info)) {
          token_to_validate = persisted_token;
        } else {
          std::erase(uploaded_context_tokens_, persisted_token);
          std::erase(submitted_context_tokens_, persisted_token);
        }
      }
    } else {
      // Flag disabled: `uploaded_context_tokens_` is cleared after each query.
      // Validate the stored token directly against the browser.
      token_to_validate = persisted_token;
    }

    if (!should_remove_from_server && signal_browser_tab_deletions &&
        tab_validator && !token_to_validate.is_empty()) {
      const auto* file_info =
          context_controller->GetFileInfo(token_to_validate);
      // If the tab is closed or this token is stale (no new token replaced it
      // after navigation), delete it from the server. If the tab is
      // closed, delete all tokens associated with the tab.
      if (file_info && !tab_validator->IsTabValidAndPointingToUrl(*file_info)) {
        should_remove_from_server = true;
      }
    }

    // Entries in `displaced_submitted_tabs_` were moved out of `attached` or
    // `restored` either by same-page recontextualization in `AddAttachedTab()`
    // (`is_superceded == true`, or replaced in `attached` by an equivalent
    // URL) or by reactive tab closure / cross-URL navigation in
    // `OnTabClosedOrNavigated()` (where `tab_validator` may not be wired up).
    // Report non-superseded displaced entries as removed when the tab is no
    // longer attached or is now attached at a non-equivalent URL.
    if (!should_remove_from_server && is_displaced &&
        signal_browser_tab_deletions) {
      const auto* persisted_file_info =
          !persisted_token.is_empty()
              ? context_controller->GetFileInfo(persisted_token)
              : nullptr;
      const bool is_superceded =
          persisted_file_info && persisted_file_info->is_superceded;
      if (!is_superceded) {
        auto attached_it = std::ranges::find_if(
            tab_context_.attached, [&](const TabInfo& attached_tab) {
              return attached_tab.tab_id == session_id.id();
            });
        if (attached_it == tab_context_.attached.end()) {
          should_remove_from_server = true;
        } else if (!tab.url.is_empty() && !attached_it->url.is_empty() &&
                   !AreUrlsEquivalent(tab.url, tab.title, attached_it->url,
                                      attached_it->title)) {
          should_remove_from_server = true;
        } else if (attached_it->context_token != persisted_token) {
          MarkFileSuperceded(context_controller, persisted_token);
        }
      }
    }

    if (!should_remove_from_server) {
      return false;
    }

    // Add tab `request_id` to server for tab removal on the server.
    bool added_removed_context = false;
    if (tab.request_id.has_value()) {
      added_removed_context =
          AppendUniqueRequestId(removed_contexts, tab.request_id.value());
    }
    MarkFileSuperceded(context_controller, persisted_token);

    // Check if there is a valid active (potentially new) token for this tab.
    // If so, the tab navigated to a new URL that has a valid replacement token,
    // so only the expired `persisted_token` should be removed below.
    bool has_valid_active_token = false;
    base::UnguessableToken active_token = GetActiveTokenForTab(session_id);
    if (tab_validator) {
      base::UnguessableToken validated_token =
          context_management_enabled ? active_token : persisted_token;
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
          has_valid_active_token = true;
        }
      }
    } else if (is_displaced) {
      // Without a `tab_validator`, a displaced tab that has been re-attached
      // with a new token is still open at its new URL.
      has_valid_active_token =
          (!active_token.is_empty() && active_token != persisted_token) ||
          std::ranges::any_of(
              tab_context_.attached, [&](const TabInfo& attached_tab) {
                return attached_tab.tab_id == session_id.id() &&
                       attached_tab.context_token != persisted_token;
              });
    }

    if (added_removed_context) {
      if (tab_context_.deselected.contains(session_id)) {
        LogRemoveContextReason(
            ContextualTasksRemovedContextReason::kUserDeselected);
      } else if (has_valid_active_token ||
                 (tab_validator && tab_validator->IsTabOpen(session_id))) {
        LogRemoveContextReason(
            ContextualTasksRemovedContextReason::kBrowserTabNavigated);
      } else {
        LogRemoveContextReason(
            ContextualTasksRemovedContextReason::kBrowserTabClosed);
      }
    }

    if (!has_valid_active_token) {
      // If closed or no valid active replacement token exists, remove all tab
      // tokens associated with this tab session id.
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
      std::erase(uploaded_context_tokens_, persisted_token);
      std::erase(submitted_context_tokens_, persisted_token);
    }
    return true;
  };

  // Entries in `displaced_submitted_tabs_` may have been displaced in
  // `AddAttachedTab()` by same-page recontextualization (`is_superceded ==
  // true`, not added to `removed_contexts`) or by cross-URL navigation /
  // `DeleteFile()` (added to `removed_contexts`). `maybe_remove_persisted_tab`
  // handles both the check and the shared controller/token cleanup.
  for (const TabInfo& tab : displaced_submitted_tabs_) {
    maybe_remove_persisted_tab(tab, /*is_displaced=*/true);
  }
  displaced_submitted_tabs_.clear();

  if (std::erase_if(tab_context_.attached, [&](const TabInfo& tab) {
        return maybe_remove_persisted_tab(tab, /*is_displaced=*/false);
      }) > 0) {
    tab_context_dirty_ = true;
  }
  MaybeNotifyTabContextSubscribers();

  base::UmaHistogramCounts100("ContextualTasks.RemovedContext.CountPerQuery",
                              removed_contexts.size());

  return removed_contexts;
}

std::vector<base::UnguessableToken>
ContextualSearchSessionHandle::MarkQuerySubmitted(
    std::vector<base::UnguessableToken> file_tokens,
    std::optional<size_t> query_text_length) {
  // Append the uploaded tokens, preserving upload order while deduplicating
  // any tokens already in `file_tokens`.
  for (const auto& token : uploaded_context_tokens_) {
    if (!std::ranges::contains(file_tokens, token)) {
      file_tokens.push_back(token);
    }
  }
  // Keep tabs but clear the files. Move any tab tokens in current
  // turn/submission into `tab_context_.attached`.
  ClearFiles(/*query_submitted=*/true);

  // Record the total number of persisted tabs attached to the session for this
  // turn (after `TakeRemovedContexts()` pruned any removed tabs and
  // `ClearFiles(/*query_submitted=*/true)` promoted newly submitted tabs).
  base::UmaHistogramCounts100(
      "ContextualTasks.RemovedContext.RemainingTabsCount",
      GetPersistedTabs().size());

  if (!file_tokens.empty()) {
    has_submitted_context_ = true;
    if (std::ranges::any_of(TokensToFileInfos(GetController(), file_tokens),
                            IsPdfFileInfo)) {
      has_submitted_pdf_context_ = true;
    }
  }

  // Copy the tokens from this request to the list of all submitted tokens.
  submitted_context_tokens_.insert(submitted_context_tokens_.end(),
                                   file_tokens.begin(), file_tokens.end());

  if (query_text_length.has_value() && GetMetricsRecorder()) {
    NotifyQuerySubmittedSessionState(
        TokensToFileInfos(GetController(), file_tokens), *query_text_length);
  }

  MaybeNotifyTabContextSubscribers();
  return file_tokens;
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

  for (const auto& req_id : TakeRemovedContexts()) {
    AppendUniqueRequestId(create_client_to_aim_request_info->removed_contexts,
                          req_id);
  }

  create_client_to_aim_request_info->file_tokens = MarkQuerySubmitted(
      std::move(create_client_to_aim_request_info->file_tokens),
      create_client_to_aim_request_info->query_text.size());

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

std::vector<base::UnguessableToken>
ContextualSearchSessionHandle::GetActiveSubmittedContextTokens() const {
  std::vector<base::UnguessableToken> tokens = submitted_context_tokens_;
  for (const TabInfo& tab : tab_context_.attached) {
    if (!tab.submitted || !tab.tab_id.has_value() ||
        tab_context_.deselected.contains(
            SessionID::FromSerializedValue(*tab.tab_id))) {
      continue;
    }
    if (!std::ranges::contains(tokens, tab.context_token)) {
      tokens.push_back(tab.context_token);
    }
  }
  return tokens;
}

std::vector<FileInfo>
ContextualSearchSessionHandle::GetUploadedContextFileInfos() const {
  return TokensToFileInfos(GetController(), uploaded_context_tokens_);
}

std::vector<FileInfo>
ContextualSearchSessionHandle::GetSubmittedContextFileInfos() const {
  return TokensToFileInfos(GetController(), GetActiveSubmittedContextTokens());
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
  if (std::ranges::any_of(
          TokensToFileInfos(GetController(), submitted_context_tokens_),
          IsPdfFileInfo)) {
    has_submitted_pdf_context_ = true;
  }
  submitted_context_tokens_.clear();
}

void ContextualSearchSessionHandle::SetSubmittedContextTokens(
    const std::vector<base::UnguessableToken>& tokens) {
  if (!tokens.empty()) {
    has_submitted_context_ = true;
    if (std::ranges::any_of(TokensToFileInfos(GetController(), tokens),
                            IsPdfFileInfo)) {
      has_submitted_pdf_context_ = true;
    }
  }
  submitted_context_tokens_ = tokens;
}

ContextualSearchSessionHandle::PersistedTabsMap
ContextualSearchSessionHandle::GetPersistedTabs() const {
  PersistedTabsMap persisted;
  for (const TabInfo& tab : tab_context_.attached) {
    if (!tab.submitted || !tab.tab_id.has_value() ||
        !tab.request_id.has_value()) {
      continue;
    }
    persisted.emplace(
        SessionID::FromSerializedValue(*tab.tab_id),
        std::make_pair(tab.context_token, tab.request_id.value()));
  }
  return persisted;
}

void ContextualSearchSessionHandle::ClearAllPersistedTabs() {
  displaced_submitted_tabs_.clear();
  if (std::erase_if(tab_context_.attached,
                    [](const TabInfo& tab) { return tab.submitted; }) > 0) {
    tab_context_dirty_ = true;
  }
}

void ContextualSearchSessionHandle::SetPersistedTabs(
    PersistedTabsMap new_persisted_tabs) {
  base::flat_map<int32_t, std::pair<GURL, std::string>> existing_tab_metadata;
  for (const TabInfo& tab : tab_context_.attached) {
    if (tab.tab_id.has_value() && (!tab.url.is_empty() || !tab.title.empty())) {
      existing_tab_metadata[*tab.tab_id] = {tab.url, tab.title};
    }
  }
  ClearAllPersistedTabs();
  auto* controller = GetController();
  for (auto& [session_id, token_and_req] : new_persisted_tabs) {
    TabInfo& tab = tab_context_.attached.emplace_back();
    tab.tab_id = session_id.id();
    tab.context_token = token_and_req.first;
    tab.request_id = std::move(token_and_req.second);
    if (const auto* file_info =
            controller ? controller->GetFileInfo(token_and_req.first)
                       : nullptr) {
      if (file_info->tab_url.has_value()) {
        tab.url = *file_info->tab_url;
      }
      if (file_info->tab_title.has_value()) {
        tab.title = *file_info->tab_title;
      }
    }
    if (auto it = existing_tab_metadata.find(session_id.id());
        it != existing_tab_metadata.end()) {
      if (tab.url.is_empty()) {
        tab.url = it->second.first;
      }
      if (tab.title.empty()) {
        tab.title = it->second.second;
      }
    }
    tab.uploaded = true;
    tab.submitted = true;
    tab_context_dirty_ = true;
  }
  MaybeNotifyTabContextSubscribers();
}

base::CallbackListSubscription
ContextualSearchSessionHandle::SubscribeTabContext(
    TabContextSubscriber callback) {
  if (!callback) {
    return {};
  }
  return tab_context_subscribers_.Add(std::move(callback));
}

const TabContextState& ContextualSearchSessionHandle::GetTabContextState()
    const {
  return tab_context_;
}

base::flat_set<SessionID>
ContextualSearchSessionHandle::GetNonDeselectedTabSessionIds() const {
  base::flat_set<SessionID> tab_ids;
  auto collect_from = [&](const std::vector<TabInfo>& tabs) {
    for (const auto& tab : tabs) {
      if (!tab.tab_id.has_value()) {
        continue;
      }
      SessionID id = SessionID::FromSerializedValue(*tab.tab_id);
      if (id.is_valid() && !tab_context_.deselected.contains(id)) {
        tab_ids.insert(id);
      }
    }
  };
  collect_from(tab_context_.attached);
  collect_from(tab_context_.restored);
  return tab_ids;
}

void ContextualSearchSessionHandle::SetRestoredTabs(std::vector<TabInfo> tabs) {
  std::vector<TabInfo> rebuilt_restored;
  rebuilt_restored.reserve(tabs.size());
  std::set<int32_t> seen_tab_ids;
  for (TabInfo& tab : tabs) {
    // Keep the first entry if the server returns the same tab more than once.
    if (tab.tab_id.has_value() && !seen_tab_ids.insert(*tab.tab_id).second) {
      continue;
    }
    tab.restored_from_aim = true;
    tab.uploaded = false;
    rebuilt_restored.push_back(std::move(tab));
  }
  if (rebuilt_restored == tab_context_.restored) {
    return;
  }
  tab_context_.restored = std::move(rebuilt_restored);
  tab_context_dirty_ = true;
  MaybeNotifyTabContextSubscribers();
}

void ContextualSearchSessionHandle::AttachTabPendingUpload(
    const base::UnguessableToken& file_token,
    int32_t tab_id,
    const GURL& url,
    const std::string& title) {
  // Exit early if the token was deleted before the delayed tab was recorded.
  if (std::ranges::find(uploaded_context_tokens_, file_token) ==
      uploaded_context_tokens_.end()) {
    return;
  }
  TabInfo tab;
  tab.context_token = file_token;
  tab.tab_id = tab_id;
  tab.url = url;
  tab.title = title;
  // The upload happens later via `StartTabContextUploadFlow()`, which
  // updates `tab.uploaded` to true.
  tab.uploaded = false;
  AddAttachedTab(std::move(tab));
}

base::UnguessableToken ContextualSearchSessionHandle::AddPendingDelayedTab(
    int32_t tab_id,
    const GURL& url,
    const std::string& title) {
  const SessionID session_id = SessionID::FromSerializedValue(tab_id);
  // Like `StartTabContextUploadFlow()`, re-adding a tab clears its deselection.
  // TODO(crbug.com/562939095): If a deselected submitted tab is re-added here
  // and its chip is deleted before submit, its removal is no longer reported,
  // since `TakeRemovedContexts()` checks the deselection. Keep the deselection
  // until the re-added tab is uploaded via `StartTabContextUploadFlow()`.
  tab_context_.deselected.erase(session_id);
  TabInfo tab;
  // Not added to `uploaded_context_tokens_`: the tab is uploaded with another
  // token via `StartTabContextUploadFlow()`, which replaces this entry.
  tab.context_token = base::UnguessableToken::Create();
  tab.tab_id = tab_id;
  tab.url = url;
  tab.title = title;
  tab.uploaded = false;
  tab.pending_delayed_upload = true;
  const base::UnguessableToken token = tab.context_token;
  delayed_tab_token_ids_.insert_or_assign(token, session_id);
  AddAttachedTab(std::move(tab));
  return token;
}

std::vector<SessionID> ContextualSearchSessionHandle::GetPendingDelayedTabIds()
    const {
  std::vector<SessionID> tab_ids;
  for (const auto& tab : tab_context_.attached) {
    if (tab.pending_delayed_upload && tab.tab_id.has_value()) {
      tab_ids.push_back(SessionID::FromSerializedValue(*tab.tab_id));
    }
  }
  return tab_ids;
}

std::optional<int32_t> ContextualSearchSessionHandle::DeleteTabContext(
    const base::UnguessableToken& token) {
  // Superseded tab uploads keep their tab in their file info, and delayed tab
  // tokens in `delayed_tab_token_ids_`.
  std::optional<SessionID> session_id;
  const auto* file_info =
      GetController() ? GetController()->GetFileInfo(token) : nullptr;
  if (file_info) {
    session_id = file_info->tab_session_id;
  } else if (auto it = delayed_tab_token_ids_.find(token);
             it != delayed_tab_token_ids_.end()) {
    session_id = it->second;
  }

  std::optional<int32_t> tab_id;
  base::UnguessableToken current_token;
  auto attached_tab =
      std::ranges::find_if(tab_context_.attached, [&](const TabInfo& tab) {
        return tab.context_token == token ||
               (session_id && tab.tab_id == session_id->id());
      });
  if (attached_tab != tab_context_.attached.end()) {
    tab_id = attached_tab->tab_id;
    if (attached_tab->context_token != token) {
      current_token = attached_tab->context_token;
    }
  }
  if (!tab_id.has_value() && session_id.has_value()) {
    tab_id = session_id->id();
  }

  auto delete_token = [this](const base::UnguessableToken& token_to_delete) {
    if (!DeleteFile(token_to_delete)) {
      std::erase(submitted_context_tokens_, token_to_delete);
      RemoveUploadedContextToken(token_to_delete);
    }
  };
  delete_token(token);
  // With tab deselection, DeleteFile() also deletes the tab's other tokens,
  // which may include `current_token`. Deleting it again would delete a
  // submitted token from the controller, so only delete it if still listed.
  if (!current_token.is_empty() &&
      (std::ranges::contains(uploaded_context_tokens_, current_token) ||
       std::ranges::contains(submitted_context_tokens_, current_token))) {
    delete_token(current_token);
  }
  if (tab_id.has_value() &&
      std::erase_if(tab_context_.restored, [&](const TabInfo& tab) {
        return tab.tab_id == tab_id;
      }) > 0) {
    NotifyTabContextSubscribers();
  }
  return tab_id;
}

std::optional<std::vector<base::UnguessableToken>>
ContextualSearchSessionHandle::DeleteTabContext(
    SessionID session_id,
    const GURL& current_url,
    const std::string& current_title) {
  if (!session_id.is_valid()) {
    return std::nullopt;
  }
  const int32_t session_tab_id = session_id.id();
  const bool in_attached =
      std::ranges::any_of(tab_context_.attached, [&](const TabInfo& tab_info) {
        return tab_info.tab_id == session_tab_id;
      });
  const bool in_restored =
      std::ranges::any_of(tab_context_.restored, [&](const TabInfo& tab_info) {
        return tab_info.tab_id == session_tab_id;
      });
  if (!in_attached && !in_restored) {
    return std::nullopt;
  }

  GURL tab_url = current_url;
  std::string tab_title = current_title;
  std::vector<base::UnguessableToken> tokens_to_remove;
  auto add_unique_valid_token = [&](const base::UnguessableToken& token) {
    if (!token.is_empty() && !std::ranges::contains(tokens_to_remove, token)) {
      tokens_to_remove.push_back(token);
    }
  };

  bool was_submitted_or_restored = in_restored;
  add_unique_valid_token(GetTokenForTab(session_id));
  for (const auto& attached_tab : tab_context_.attached) {
    if (attached_tab.tab_id == session_tab_id) {
      add_unique_valid_token(attached_tab.context_token);
      if (!tab_url.is_valid() && attached_tab.url.is_valid()) {
        tab_url = attached_tab.url;
        tab_title = attached_tab.title;
      }
      if (attached_tab.submitted) {
        was_submitted_or_restored = true;
      }
    }
  }
  for (const auto& restored_tab : tab_context_.restored) {
    if (restored_tab.tab_id == session_tab_id && !tab_url.is_valid() &&
        restored_tab.url.is_valid()) {
      tab_url = restored_tab.url;
      tab_title = restored_tab.title;
    }
  }
  for (const auto& file_info : GetUploadedContextFileInfos()) {
    if (file_info.tab_session_id == session_id) {
      add_unique_valid_token(file_info.file_token);
    }
  }
  std::optional<TabInfo> submitted_restored_tab;
  for (const auto& file_info : GetSubmittedContextFileInfos()) {
    if (file_info.tab_session_id == session_id) {
      was_submitted_or_restored = true;
      add_unique_valid_token(file_info.file_token);
      if (!submitted_restored_tab.has_value() &&
          file_info.request_id.has_value()) {
        submitted_restored_tab = TabInfo{
            .context_token = file_info.file_token,
            .tab_id = session_tab_id,
            .request_id = file_info.request_id,
            .uploaded = !file_info.is_implicit_upload,
            .submitted = true,
        };
      }
    }
  }

  const bool deselection_enabled = IsTabDeselectionEnabled();
  if (deselection_enabled && was_submitted_or_restored) {
    AddDeselectedTab(session_id, tab_url, tab_title);
  }

  for (const auto& token : tokens_to_remove) {
    // When tab deselection is enabled, `DeleteFile()` cascades to other tokens
    // associated with the same `session_id`. Skip tokens that were already
    // removed by a prior cascade so submitted tokens are not deleted a second
    // time from `context_controller`.
    if (!std::ranges::contains(uploaded_context_tokens_, token) &&
        !std::ranges::contains(submitted_context_tokens_, token) &&
        !IsTokenInTabList(tab_context_.attached, token,
                          /*must_be_submitted=*/false) &&
        !delayed_tab_token_ids_.contains(token)) {
      continue;
    }
    if (!DeleteFile(token)) {
      RemoveUploadedContextToken(token);
    }
  }

  const bool restored_removed =
      std::erase_if(tab_context_.restored,
                    [session_tab_id](const TabInfo& tab) {
                      return tab.tab_id == session_tab_id;
                    }) > 0;

  if (deselection_enabled && was_submitted_or_restored) {
    const bool already_displaced = std::ranges::any_of(
        displaced_submitted_tabs_, [session_tab_id](const TabInfo& tab) {
          return tab.tab_id == session_tab_id;
        });
    if (!already_displaced && submitted_restored_tab.has_value()) {
      displaced_submitted_tabs_.push_back(std::move(*submitted_restored_tab));
    }
    std::erase_if(submitted_context_tokens_,
                  [&](const base::UnguessableToken& token) {
                    return std::ranges::contains(tokens_to_remove, token);
                  });
  }

  if (restored_removed) {
    NotifyTabContextSubscribers();
  }

  return tokens_to_remove;
}

std::vector<base::UnguessableToken>
ContextualSearchSessionHandle::OnTabClosedOrNavigated(
    SessionID tab_session_id,
    std::optional<GURL> current_url,
    std::optional<std::string> current_title) {
  std::vector<base::UnguessableToken> removed_tokens;
  if (!tab_session_id.is_valid() ||
      !base::FeatureList::IsEnabled(
          lens::features::kLensDeleteContextOnPageNavigation)) {
    return removed_tokens;
  }

  auto* context_controller = GetController();
  auto is_url_equivalent_to_current = [&](const GURL& url,
                                          const std::string& title) {
    if (!current_url.has_value() || !current_url->is_valid() ||
        url.is_empty() || !url.is_valid()) {
      return false;
    }
    return AreUrlsEquivalent(url, title, *current_url,
                             current_title.value_or(""));
  };

  auto append_unique_token = [&](const base::UnguessableToken& token) {
    if (!token.is_empty() && !std::ranges::contains(removed_tokens, token)) {
      removed_tokens.push_back(token);
    }
  };

  bool tab_context_changed = false;

  // 1. Inspect `tab_context_.attached`.
  if (std::erase_if(tab_context_.attached, [&](TabInfo& tab) {
        if (tab.tab_id != tab_session_id.id()) {
          return false;
        }
        GURL tab_url = tab.url;
        std::string tab_title = tab.title;
        const FileInfo* info =
            (context_controller && !tab.context_token.is_empty())
                ? context_controller->GetFileInfo(tab.context_token)
                : nullptr;
        if (tab_url.is_empty() && info && info->tab_url.has_value()) {
          tab_url = *info->tab_url;
          tab_title = info->tab_title.value_or("");
        }
        if (is_url_equivalent_to_current(tab_url, tab_title)) {
          return false;
        }

        append_unique_token(tab.context_token);
        delayed_tab_token_ids_.erase(tab.context_token);
        std::erase(uploaded_context_tokens_, tab.context_token);
        const bool was_submitted =
            tab.submitted ||
            std::ranges::contains(submitted_context_tokens_, tab.context_token);
        std::erase(submitted_context_tokens_, tab.context_token);
        if (was_submitted) {
          tab.submitted = true;
          if (!tab.request_id.has_value() && info &&
              info->request_id.has_value()) {
            tab.request_id = info->request_id;
          }
          if (tab.url.is_empty()) {
            tab.url = tab_url;
            tab.title = tab_title;
          }
          displaced_submitted_tabs_.push_back(std::move(tab));
        } else if (info) {
          context_controller->DeleteFile(tab.context_token);
        }
        return true;
      }) > 0) {
    tab_context_changed = true;
  }

  // 2. Clean up any remaining uploaded or submitted tokens for this tab in the
  // controller (e.g., if tokens were added without `tab_context_.attached`).
  if (context_controller) {
    std::vector<base::UnguessableToken> uploaded_to_delete;
    std::erase_if(uploaded_context_tokens_, [&](const auto& token) {
      const auto* info = context_controller->GetFileInfo(token);
      if (!info || info->tab_session_id != tab_session_id) {
        return false;
      }
      if (info->tab_url.has_value() &&
          is_url_equivalent_to_current(*info->tab_url,
                                       info->tab_title.value_or(""))) {
        return false;
      }
      append_unique_token(token);
      if (!std::ranges::contains(submitted_context_tokens_, token) &&
          !IsTokenInTabList(displaced_submitted_tabs_, token,
                            /*must_be_submitted=*/false)) {
        uploaded_to_delete.push_back(token);
      }
      return true;
    });
    for (const auto& token : uploaded_to_delete) {
      context_controller->DeleteFile(token);
    }

    std::erase_if(submitted_context_tokens_, [&](const auto& token) {
      const auto* info = context_controller->GetFileInfo(token);
      if (!info || info->tab_session_id != tab_session_id) {
        return false;
      }
      if (info->tab_url.has_value() &&
          is_url_equivalent_to_current(*info->tab_url,
                                       info->tab_title.value_or(""))) {
        return false;
      }
      append_unique_token(token);
      if (!IsTokenInTabList(displaced_submitted_tabs_, token,
                            /*must_be_submitted=*/false)) {
        TabInfo displaced;
        displaced.tab_id = tab_session_id.id();
        displaced.context_token = token;
        displaced.request_id = info->request_id;
        displaced.url = info->tab_url.value_or(GURL());
        displaced.title = info->tab_title.value_or("");
        displaced.submitted = true;
        displaced_submitted_tabs_.push_back(std::move(displaced));
      }
      return true;
    });
  }

  // 3. Clean up any orphaned `delayed_tab_token_ids_` for this tab.
  const bool tab_still_attached = std::ranges::any_of(
      tab_context_.attached, [&](const TabInfo& attached_tab) {
        return attached_tab.tab_id == tab_session_id.id();
      });
  if (!tab_still_attached) {
    base::EraseIf(delayed_tab_token_ids_, [&](const auto& entry) {
      if (entry.second != tab_session_id) {
        return false;
      }
      append_unique_token(entry.first);
      return true;
    });
  }

  // 4. Inspect `tab_context_.restored`.
  if (std::erase_if(tab_context_.restored, [&](TabInfo& tab) {
        if (tab.tab_id != tab_session_id.id()) {
          return false;
        }
        if (is_url_equivalent_to_current(tab.url, tab.title)) {
          return false;
        }
        const bool already_displaced = std::ranges::any_of(
            displaced_submitted_tabs_, [&](const TabInfo& displaced) {
              return displaced.tab_id == tab_session_id.id();
            });
        if (!already_displaced) {
          tab.submitted = true;
          displaced_submitted_tabs_.push_back(std::move(tab));
        }
        return true;
      }) > 0) {
    tab_context_changed = true;
  }

  // 5. Clear stale deselection entry if the tab closed or navigated away.
  if (auto it = tab_context_.deselected.find(tab_session_id);
      it != tab_context_.deselected.end()) {
    if (!is_url_equivalent_to_current(it->second.first, it->second.second)) {
      tab_context_.deselected.erase(it);
    }
  }

  if (tab_context_changed) {
    NotifyTabContextSubscribers();
  }

  return removed_tokens;
}

void ContextualSearchSessionHandle::AddAttachedTab(TabInfo tab) {
  CHECK(tab.tab_id.has_value());
  tab.restored_from_aim = false;
  auto it =
      std::ranges::find(tab_context_.attached, tab.tab_id, &TabInfo::tab_id);
  // De-duplicate tabs.
  if (it == tab_context_.attached.end()) {
    tab_context_.attached.push_back(std::move(tab));
  } else if (*it == tab) {
    return;
  } else {
    if (it->submitted) {
      tab.uploaded = tab.uploaded || it->uploaded;
    }
    if (it->submitted && it->context_token != tab.context_token) {
      displaced_submitted_tabs_.push_back(std::move(*it));
    }
    *it = std::move(tab);
  }
  NotifyTabContextSubscribers();
}

void ContextualSearchSessionHandle::RemoveAttachedTabForToken(
    const base::UnguessableToken& file_token) {
  std::optional<int32_t> removed_tab_id;
  bool changed = std::erase_if(tab_context_.attached, [&](TabInfo& tab) {
                   if (tab.context_token != file_token) {
                     return false;
                   }
                   removed_tab_id = tab.tab_id;
                   if (tab.submitted && tab.request_id.has_value()) {
                     displaced_submitted_tabs_.push_back(std::move(tab));
                   }
                   return true;
                 }) > 0;
  if (removed_tab_id.has_value()) {
    changed |= std::erase_if(tab_context_.restored, [&](const TabInfo& tab) {
                 return tab.tab_id == *removed_tab_id;
               }) > 0;
  }
  if (!changed) {
    return;
  }
  NotifyTabContextSubscribers();
}

void ContextualSearchSessionHandle::NotifyTabContextSubscribers() {
  tab_context_subscribers_.Notify(tab_context_);
}

void ContextualSearchSessionHandle::MaybeNotifyTabContextSubscribers() {
  if (!tab_context_dirty_) {
    return;
  }
  tab_context_dirty_ = false;
  NotifyTabContextSubscribers();
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

bool ContextualSearchSessionHandle::HasPdfContext() const {
  return has_submitted_pdf_context_ ||
         std::ranges::any_of(GetSubmittedContextFileInfos(), IsPdfFileInfo) ||
         std::ranges::any_of(GetUploadedContextFileInfos(), IsPdfFileInfo);
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
    auto it = std::ranges::find_if(
        tab_context_.attached,
        [&](const TabInfo& tab) { return tab.tab_id == tab_session_id.id(); });
    if (it != tab_context_.attached.end() && it->submitted) {
      return it->context_token;
    }
  }

  return base::UnguessableToken();
}

void ContextualSearchSessionHandle::MarkTabAckByServer(
    const base::UnguessableToken& token) {
  if (!IsTabToken(token)) {
    return;
  }
  auto* controller = GetController();
  if (!controller) {
    return;
  }
  const auto* file_info = controller->GetFileInfo(token);
  if (!file_info || file_info->is_superceded) {
    return;
  }
  // Request ID must exist, as the tab was already submitted to server.
  CHECK(file_info->request_id.has_value());
  const int32_t tab_id = file_info->tab_session_id->id();
  auto tab = std::ranges::find_if(
      tab_context_.attached,
      [&](const TabInfo& info) { return info.tab_id == tab_id; });
  if (tab == tab_context_.attached.end()) {
    tab = tab_context_.attached.emplace(tab_context_.attached.end());
    tab->tab_id = tab_id;
  }
  // The token may be newer than the one previously marked, e.g. after
  // recontextualization.
  tab->context_token = token;
  tab->request_id = file_info->request_id;
  if (file_info->tab_url.has_value()) {
    tab->url = *file_info->tab_url;
  }
  if (file_info->tab_title.has_value()) {
    tab->title = *file_info->tab_title;
  }
  tab->uploaded = tab->uploaded || !file_info->is_implicit_upload;
  tab->submitted = true;
  tab_context_dirty_ = true;
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
  auto it = std::ranges::find_if(
      tab_context_.attached,
      [&](const TabInfo& tab) { return tab.tab_id == tab_session_id.id(); });
  if (it != tab_context_.attached.end() && it->submitted) {
    return it->context_token;
  }
  return base::UnguessableToken();
}

bool ContextualSearchSessionHandle::IsTabDeselected(
    SessionID tab_session_id,
    const GURL& current_url,
    const std::string& current_title) const {
  auto it = tab_context_.deselected.find(tab_session_id);
  if (it == tab_context_.deselected.end()) {
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
    tab_context_.deselected.erase(it);
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

void ContextualSearchSessionHandle::AddDeselectedTab(SessionID tab_session_id,
                                                     const GURL& url,
                                                     const std::string& title) {
  tab_context_.deselected.insert_or_assign(tab_session_id,
                                           std::make_pair(url, title));
}

void ContextualSearchSessionHandle::RemoveDeselectedTab(
    SessionID tab_session_id) {
  tab_context_.deselected.erase(tab_session_id);
}

void ContextualSearchSessionHandle::set_auth_user_index(
    size_t auth_user_index) {
  auth_user_index_ = auth_user_index;
  if (auto* controller = GetController()) {
    controller->SetAuthUserIndex(auth_user_index);
  }
}

}  // namespace contextual_search
