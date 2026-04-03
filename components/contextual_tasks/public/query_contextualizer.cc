// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/query_contextualizer.h"

#include "base/barrier_closure.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/utils.h"
#include "components/lens/lens_features.h"
#include "components/url_deduplication/url_deduplication_helper.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/gfx/skia_util.h"
#include "url/gurl.h"

namespace contextual_tasks {

namespace {
// The amount of change in bytes that is considered a significant change and
// should trigger a page content update request. This provides tolerance in
// case there is slight variation in the retrieved bytes in between calls.
constexpr float kByteChangeTolerancePercent = 0.01;
}  // namespace

QueryContextualizer::QueryContextualizer(ContextualTasksService* service,
                                         Delegate* delegate)
    : service_(service), delegate_(delegate) {
  DCHECK(service_);
  DCHECK(delegate_);
}

QueryContextualizer::~QueryContextualizer() = default;

void QueryContextualizer::Contextualize(
    const std::optional<base::Uuid>& task_id,
    const std::string& query_text,
    const std::vector<TabId>& tabs_to_recontextualize,
    const std::vector<TabId>& tabs_to_force_contextualize,
    contextual_search::ContextualSearchSessionHandle* session_handle,
    base::OnceClosure callback) {
  auto context_decoration_params = std::make_unique<ContextDecorationParams>();
  if (session_handle) {
    context_decoration_params->contextual_search_session_handle =
        session_handle->AsWeakPtr();
  }

  if (!task_id.has_value()) {
    OnContextRetrieved(/*task_id=*/std::nullopt, query_text,
                       tabs_to_recontextualize, tabs_to_force_contextualize,
                       session_handle ? session_handle->AsWeakPtr() : nullptr,
                       std::move(callback), /*context=*/nullptr);
    return;
  }

  service_->GetContextForTask(
      task_id.value(),
      {ContextualTaskContextSource::kSubmittedContextDecorator},
      std::move(context_decoration_params),
      base::BindOnce(&QueryContextualizer::OnContextRetrieved,
                     weak_factory_.GetWeakPtr(), task_id, query_text,
                     tabs_to_recontextualize, tabs_to_force_contextualize,
                     session_handle ? session_handle->AsWeakPtr() : nullptr,
                     std::move(callback)));
}

void QueryContextualizer::OnContextRetrieved(
    const std::optional<base::Uuid>& task_id,
    const std::string& query_text,
    const std::vector<TabId>& tabs_to_recontextualize,
    const std::vector<TabId>& tabs_to_force_contextualize,
    base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
        session_handle,
    base::OnceClosure callback,
    std::unique_ptr<ContextualTaskContext> context) {
  // Fail early if the task id was specified but there was no context for the
  // task. This indicates that the task was not available (i.e. was deleted)
  // and no further action is needed.
  if (task_id.has_value() && !context) {
    std::move(callback).Run();
    return;
  }

  // Extract URLs from the query text and start upload flows for them.
  if (session_handle && lens::features::IsLensSendUrlsInComposeboxesEnabled()) {
    re2::StringPiece input(query_text);
    std::string url_str;
    // Regex to extract URLs.
    // Matches http://, https://, ftp://, or www. followed by valid URL
    // characters. Explicitly lists allowed characters instead of using ranges
    // like #-; for readability. Allowed characters: alphanumeric, -, ., ~, :,
    // /, ?, #, [, ], @, !, $, &, ', (, ), *, +, ,, ;, =, %
    static const base::NoDestructor<re2::RE2> url_regex(
        R"((?i)((?:(?:https?|ftp)://|www\.)[\w#$%'()*+,\-./:;!=?@\[\]_`{|}~]+))");

    std::vector<GURL> extracted_urls;
    base::flat_set<GURL> seen_urls;

    while (RE2::FindAndConsume(&input, *url_regex, &url_str)) {
      GURL url;
      if (base::StartsWith(url_str, "www.",
                           base::CompareCase::INSENSITIVE_ASCII)) {
        url = GURL("http://" + url_str);
      } else {
        url = GURL(url_str);
      }
      if (url.is_valid() && seen_urls.insert(url).second) {
        extracted_urls.push_back(url);
      }
    }

    for (const GURL& url : extracted_urls) {
      // TODO(crbug.com/495601934): QueryContextualizer should wait for all
      // uploads (including tabs and URL uploads) to complete before running the
      // callback.
      session_handle->StartUrlContextUploadFlow(
          session_handle->CreateContextToken(), url);
    }
  }

  std::vector<TabUpdate> tabs_to_update = GetTabsToUpdate(
      context.get(), tabs_to_recontextualize, tabs_to_force_contextualize);

  if (tabs_to_update.empty()) {
    std::move(callback).Run();
    return;
  }

  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(tabs_to_update.size(), std::move(callback));

  for (const TabUpdate& update : tabs_to_update) {
    delegate_->GetPageContext(
        update.id,
        base::BindOnce(&QueryContextualizer::OnTabContextualizationFetched,
                       weak_factory_.GetWeakPtr(), task_id,
                       context
                           ? std::make_unique<ContextualTaskContext>(*context)
                           : nullptr,
                       barrier_closure, update.id,
                       update.is_recontextualization, session_handle));
  }
}

void QueryContextualizer::OnTabContextualizationFetched(
    const std::optional<base::Uuid>& task_id,
    std::unique_ptr<ContextualTaskContext> context,
    base::RepeatingClosure barrier_closure,
    TabId tab_id,
    bool is_recontextualization,
    base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
        session_handle,
    std::unique_ptr<lens::ContextualInputData> page_content_data) {
  if (!page_content_data) {
    delegate_->OnTabProcessedForQueryContextualization(tab_id);
    barrier_closure.Run();
    return;
  }

  page_content_data->is_implicit_upload = is_recontextualization;

  if (GetIsProtectedPageErrorEnabled() &&
      !page_content_data->is_page_context_eligible.value_or(false)) {
    delegate_->OnPageContextIneligible();
    delegate_->OnTabProcessedForQueryContextualization(tab_id);
    barrier_closure.Run();
    return;
  }

  std::optional<int64_t> maybe_context_id = std::nullopt;
  if (context && page_content_data->tab_session_id.has_value()) {
    maybe_context_id =
        GetContextIdForTab(*context, *page_content_data, session_handle);
  }

  if (CheckIfContextChangedAndPrepareUploadData(
          maybe_context_id, *page_content_data, session_handle)) {
    delegate_->OnTabProcessedForQueryContextualization(tab_id);
    barrier_closure.Run();
    return;
  }

  delegate_->UploadTabContextWithData(
      tab_id, maybe_context_id, std::move(page_content_data),
      base::BindOnce(
          [](base::WeakPtr<QueryContextualizer> orchestrator, TabId id,
             base::RepeatingClosure barrier, bool success) {
            if (orchestrator) {
              orchestrator->delegate_->OnTabProcessedForQueryContextualization(
                  id);
            }
            barrier.Run();
          },
          weak_factory_.GetWeakPtr(), tab_id, barrier_closure));
}

std::vector<QueryContextualizer::TabUpdate>
QueryContextualizer::GetTabsToUpdate(
    const ContextualTaskContext* context,
    const std::vector<TabId>& tabs_to_recontextualize,
    const std::vector<TabId>& tabs_to_force_contextualize) {
  std::vector<TabUpdate> tabs_to_update;
  std::set<TabId> added_tabs;

  for (TabId id : tabs_to_force_contextualize) {
    if (!added_tabs.contains(id)) {
      tabs_to_update.push_back({id, /*is_recontextualization=*/false});
      added_tabs.insert(id);
    }
  }

  // Support cases in which the contextual task context is not yet available.
  if (!context) {
    return tabs_to_update;
  }

  for (TabId id : tabs_to_recontextualize) {
    if (added_tabs.contains(id)) {
      continue;
    }

    GURL url = delegate_->GetTabUrl(id);
    SessionID session_id = delegate_->GetTabSessionId(id);

    if (GetMatchingAttachment(*context, url, session_id)) {
      tabs_to_update.push_back({id, /*is_recontextualization=*/true});
      added_tabs.insert(id);
    }
  }

  return tabs_to_update;
}

std::optional<int64_t> QueryContextualizer::GetContextIdForTab(
    const ContextualTaskContext& context,
    const lens::ContextualInputData& page_content_data,
    base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
        session_handle) {
  if (!page_content_data.tab_session_id.has_value()) {
    return std::nullopt;
  }
  SessionID tab_session_id = page_content_data.tab_session_id.value();

  if (!page_content_data.page_url.has_value()) {
    return std::nullopt;
  }

  if (GetMatchingAttachment(context, page_content_data.page_url.value(),
                            tab_session_id)) {
    auto* search_context_controller =
        session_handle ? session_handle->GetController() : nullptr;
    if (search_context_controller) {
      const auto& file_info_list = search_context_controller->GetFileInfoList();
      for (const auto* file_info : file_info_list) {
        if (file_info->tab_session_id == tab_session_id &&
            !file_info->is_superceded) {
          return file_info->GetContextId();
        }
      }
    }
  }
  return std::nullopt;
}

bool QueryContextualizer::CheckIfContextChangedAndPrepareUploadData(
    std::optional<int64_t> context_id,
    lens::ContextualInputData& page_content_data,
    base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
        session_handle) {
  if (!context_id.has_value()) {
    return false;
  }

  if (!page_content_data.tab_session_id.has_value()) {
    return false;
  }
  SessionID tab_session_id = page_content_data.tab_session_id.value();

  auto* search_context_controller =
      session_handle ? session_handle->GetController() : nullptr;
  if (!search_context_controller) {
    return false;
  }

  const auto& file_info_list = search_context_controller->GetFileInfoList();
  const contextual_search::FileInfo* matching_file_info = nullptr;
  for (const auto* file_info : file_info_list) {
    if (file_info->tab_session_id == tab_session_id &&
        !file_info->is_superceded) {
      matching_file_info = file_info;
      break;
    }
  }

  if (!matching_file_info) {
    return false;
  }

  if (matching_file_info->upload_status ==
      contextual_search::ContextUploadStatus::kUploadExpired) {
    return false;
  }

  if (!matching_file_info->input_data) {
    return false;
  }

  const auto& old_data = *matching_file_info->input_data;
  const auto& new_data = page_content_data;

  bool page_content_changed = false;
  bool viewport_changed = false;

  if (old_data.primary_content_type != new_data.primary_content_type) {
    page_content_changed = true;
  } else if (new_data.primary_content_type.has_value()) {
    const std::vector<lens::ContextualInput>& old_inputs =
        old_data.context_input.has_value()
            ? *old_data.context_input
            : std::vector<lens::ContextualInput>();
    const std::vector<lens::ContextualInput>& new_inputs =
        new_data.context_input.has_value()
            ? *new_data.context_input
            : std::vector<lens::ContextualInput>();
    auto old_it = std::ranges::find_if(old_inputs, [&](const auto& input) {
      return input.content_type_ == new_data.primary_content_type.value();
    });
    auto new_it = std::ranges::find_if(new_inputs, [&](const auto& input) {
      return input.content_type_ == new_data.primary_content_type.value();
    });

    if (old_it != old_inputs.end() && new_it != new_inputs.end()) {
      const float old_size = old_it->bytes_.size();
      const float new_size = new_it->bytes_.size();
      if (old_size > 0) {
        const float percent_changed = abs((new_size - old_size) / old_size);
        if (percent_changed >= kByteChangeTolerancePercent) {
          page_content_changed = true;
        }
      } else if (new_size > 0) {
        page_content_changed = true;
      }
    } else if (old_it != old_inputs.end() || new_it != new_inputs.end()) {
      page_content_changed = true;
    }
  }

  // Check if viewport screenshot changed.
  // TODO(crbug.com/471960792): Add support for only recontextualizing the
  // screenshot when the viewport has changed but the page contents are the
  // same.

  // The screenshot may be in either the byte array or bitmap members of
  // ContextualInputData. Both should be checked for changes.
  bool old_has_screenshot = old_data.viewport_screenshot_bytes.has_value() &&
                            !old_data.viewport_screenshot_bytes->empty();
  bool new_has_screenshot = new_data.viewport_screenshot_bytes.has_value() &&
                            !new_data.viewport_screenshot_bytes->empty();

  if (old_has_screenshot != new_has_screenshot) {
    viewport_changed = true;
  } else if (old_has_screenshot) {
    const auto& old_bytes = old_data.viewport_screenshot_bytes.value();
    const auto& new_bytes = new_data.viewport_screenshot_bytes.value();
    if (old_bytes.size() != new_bytes.size()) {
      viewport_changed = true;
    } else if (old_bytes != new_bytes) {
      // Exact byte comparison for screenshot.
      viewport_changed = true;
    }
  }

  bool old_has_bitmap = old_data.viewport_screenshot.has_value();
  bool new_has_bitmap = new_data.viewport_screenshot.has_value();

  if (old_has_bitmap != new_has_bitmap) {
    viewport_changed = true;
  } else if (old_has_bitmap) {
    const auto& old_bitmap = old_data.viewport_screenshot.value();
    const auto& new_bitmap = new_data.viewport_screenshot.value();
    if (!new_bitmap.drawsNothing() &&
        (old_bitmap.drawsNothing() ||
         !gfx::BitmapsAreEqual(old_bitmap, new_bitmap))) {
      viewport_changed = true;
    }
  }

  if (new_data.primary_content_type == lens::MimeType::kPdf) {
    page_content_changed = false;
  }

  if (!page_content_changed && !viewport_changed) {
    return true;
  }

  if (!page_content_changed) {
    page_content_data.context_input = std::nullopt;
  }

  return false;
}

const UrlAttachment* QueryContextualizer::GetMatchingAttachment(
    const ContextualTaskContext& context,
    const GURL& url,
    SessionID session_id) {
  std::unique_ptr<url_deduplication::URLDeduplicationHelper>
      url_duplication_helper = CreateURLDeduplicationHelperForContextualTask();
  std::vector<const UrlAttachment*> matching_attachments =
      context.GetMatchingUrlAttachments(url, url_duplication_helper.get());

  for (const auto* attachment : matching_attachments) {
    if (attachment->GetTabSessionId() == session_id) {
      return attachment;
    }
  }
  return nullptr;
}

}  // namespace contextual_tasks
