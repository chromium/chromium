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
#include "base/task/sequenced_task_runner.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/contextual_search_types.h"
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

// Helper class to track the upload status of multiple contexts (tabs and URLs).
// This class registers itself as an observer to the
// ContextualSearchContextController to listen for upload status changes. When
// all tracked uploads have reached a terminal state (e.g., successful or
// failed), it executes the provided callback. By inheriting from
// base::RefCounted, its lifetime is safely managed across asynchronous upload
// tracking and multiple callbacks.
class UploadTracker
    : public base::RefCounted<UploadTracker>,
      public contextual_search::ContextualSearchContextController::
          ContextUploadStatusObserver {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  explicit UploadTracker(
      contextual_search::ContextualSearchContextController* controller)
      : controller_(controller ? controller->AsWeakPtr() : nullptr) {
    if (controller_) {
      controller_->AddObserver(this);
    }
  }

  void AddToken(base::UnguessableToken token) { pending_tokens_.insert(token); }

  void NotifyUploadsStarted(
      QueryContextualizer::ContextualizedCallback callback,
      base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
          session_handle) {
    callback_ = std::move(callback);
    session_handle_ = session_handle;
    uploads_started_ = true;
    if (!pending_tokens_.empty()) {
      self_ref_ = base::WrapRefCounted(this);
    }
    CheckCompletion();
  }

  void OnContextUploadStatusChanged(
      const base::UnguessableToken& context_token,
      lens::MimeType mime_type,
      contextual_search::ContextUploadStatus status,
      const std::optional<contextual_search::ContextUploadErrorType>&
          error_type) override {
    if (contextual_search::IsTerminalContextStatus(status)) {
      pending_tokens_.erase(context_token);
      CheckCompletion();
    }
  }

  void OnControllerDestroyed() override {
    controller_ = nullptr;
    // Treat pending uploads as cancelled and finalize the process.
    pending_tokens_.clear();
    CheckCompletion();
  }

 private:
  friend class base::RefCounted<UploadTracker>;

  ~UploadTracker() override {
    if (controller_) {
      controller_->RemoveObserver(this);
    }
  }

  void CheckCompletion() {
    if (uploads_started_ && pending_tokens_.empty() && callback_) {
      if (controller_) {
        controller_->RemoveObserver(this);
        controller_ = nullptr;
      }

      // Delay destruction of `this` until after the current task (e.g. observer
      // notification loop) completes, to prevent crashes in production. We do
      // this by posting a task that captures a reference to `UploadTracker`.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::DoNothingWithBoundArgs(base::WrapRefCounted(this)));

      // Run the callback synchronously so that tests (and synchronous flows)
      // can verify it immediately.
      std::move(callback_).Run(session_handle_);
      self_ref_.reset();
    }
  }

  base::WeakPtr<contextual_search::ContextualSearchContextController>
      controller_;
  QueryContextualizer::ContextualizedCallback callback_;
  base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
      session_handle_;
  std::set<base::UnguessableToken> pending_tokens_;
  bool uploads_started_ = false;
  scoped_refptr<UploadTracker> self_ref_;
};

QueryContextualizer::QueryContextualizer(ContextualTasksService* service,
                                         Delegate* delegate)
    : service_(service), delegate_(delegate) {
  DCHECK(delegate_);
}

QueryContextualizer::~QueryContextualizer() = default;

// static
std::vector<GURL> QueryContextualizer::ExtractUrlsFromQuery(
    const std::string& query_text) {
  re2::StringPiece input(query_text);
  std::string url_str;
  // Regex to extract URLs.
  // Matches http://, https://, ftp://, or www. followed by valid URL
  // characters. Explicitly lists allowed characters instead of using ranges
  // like #-; for readability. Allowed characters: alphanumeric, -, ., ~, :,
  // /, ?, #, [, ], @, !, $, &, ', (, ), *, +, ,, ;, =, %
  static const base::NoDestructor<re2::RE2> url_regex(
      R"((?i)((?:(?:https?|ftp)://|www\.)[\w#$&%'()*+,\-./:;!=?@\[\]_`{|}~]+))");

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
  return extracted_urls;
}

void QueryContextualizer::Contextualize(
    const std::optional<base::Uuid>& task_id,
    const std::string& query_text,
    const std::vector<TabId>& tabs_to_recontextualize,
    const std::vector<TabId>& tabs_to_force_contextualize,
    PageContextIneligibleCallback on_ineligible_callback,
    TabProcessedCallback on_processed_callback,
    ContextualizedCallback callback,
    bool enable_smart_tab_selection) {
  auto context_decoration_params = std::make_unique<ContextDecorationParams>();
  base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
      session_handle;

  // If there are tabs to contextualize, or a task id is provided, get or create
  // the session handle.
  if (task_id.has_value() || !tabs_to_recontextualize.empty() ||
      !tabs_to_force_contextualize.empty() || enable_smart_tab_selection) {
    auto* handle = delegate_->GetOrCreateSessionHandleForQueryContextualizer();
    if (handle) {
      session_handle = handle->AsWeakPtr();
      context_decoration_params->contextual_search_session_handle =
          session_handle;
    }
  }

  // TODO(crbug.com/502639860): Actually using this in
  // contextual_searchbox_handler, setting enable_smart_tab_selection to true,
  // and removing legacy logic will happen in a followup CL.
  if (enable_smart_tab_selection) {
    std::vector<GURL> attached_context_urls;
    if (session_handle && session_handle->GetController()) {
      const auto& file_info_list =
          session_handle->GetController()->GetFileInfoList();
      for (const auto* file_info : file_info_list) {
        if (file_info->tab_url.has_value() && !file_info->is_superceded) {
          attached_context_urls.push_back(file_info->tab_url.value());
        }
      }
    }
    for (TabId id : tabs_to_recontextualize) {
      attached_context_urls.push_back(delegate_->GetTabUrl(id));
    }
    for (TabId id : tabs_to_force_contextualize) {
      attached_context_urls.push_back(delegate_->GetTabUrl(id));
    }

    delegate_->GetRelevantTabsForQuery(
        query_text, attached_context_urls,
        base::BindOnce(&QueryContextualizer::OnRelevantTabsFetched,
                       weak_factory_.GetWeakPtr(), task_id, query_text,
                       tabs_to_recontextualize, tabs_to_force_contextualize,
                       session_handle, on_ineligible_callback,
                       on_processed_callback, std::move(callback)));
    return;
  }

  if (!task_id.has_value() || !service_) {
    OnContextRetrieved(/*task_id=*/std::nullopt, query_text,
                       tabs_to_recontextualize, tabs_to_force_contextualize,
                       /*smart_tabs_to_contextualize=*/{}, session_handle,
                       on_ineligible_callback, on_processed_callback,
                       std::move(callback),
                       /*context=*/nullptr);
    return;
  }

  service_->GetContextForTask(
      task_id.value(),
      {ContextualTaskContextSource::kSubmittedContextDecorator},
      std::move(context_decoration_params),
      base::BindOnce(&QueryContextualizer::OnContextRetrieved,
                     weak_factory_.GetWeakPtr(), task_id, query_text,
                     tabs_to_recontextualize, tabs_to_force_contextualize,
                     /*smart_tabs_to_contextualize=*/std::vector<TabId>(),
                     session_handle, on_ineligible_callback,
                     on_processed_callback, std::move(callback)));
}

void QueryContextualizer::OnRelevantTabsFetched(
    const std::optional<base::Uuid>& task_id,
    const std::string& query_text,
    const std::vector<TabId>& tabs_to_recontextualize,
    const std::vector<TabId>& tabs_to_force_contextualize,
    base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
        session_handle,
    PageContextIneligibleCallback on_ineligible_callback,
    TabProcessedCallback on_processed_callback,
    ContextualizedCallback callback,
    std::vector<TabId> smart_tabs) {
  auto context_decoration_params = std::make_unique<ContextDecorationParams>();
  if (session_handle) {
    context_decoration_params->contextual_search_session_handle =
        session_handle;
  }

  if (!task_id.has_value() || !service_) {
    OnContextRetrieved(/*task_id=*/std::nullopt, query_text,
                       tabs_to_recontextualize, tabs_to_force_contextualize,
                       smart_tabs, session_handle, on_ineligible_callback,
                       on_processed_callback, std::move(callback),
                       /*context=*/nullptr);
    return;
  }

  service_->GetContextForTask(
      task_id.value(),
      {ContextualTaskContextSource::kSubmittedContextDecorator},
      std::move(context_decoration_params),
      base::BindOnce(&QueryContextualizer::OnContextRetrieved,
                     weak_factory_.GetWeakPtr(), task_id, query_text,
                     tabs_to_recontextualize, tabs_to_force_contextualize,
                     smart_tabs, session_handle, on_ineligible_callback,
                     on_processed_callback, std::move(callback)));
}

void QueryContextualizer::OnContextRetrieved(
    const std::optional<base::Uuid>& task_id,
    const std::string& query_text,
    const std::vector<TabId>& tabs_to_recontextualize,
    const std::vector<TabId>& tabs_to_force_contextualize,
    const std::vector<TabId>& smart_tabs_to_contextualize,
    base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
        session_handle,
    PageContextIneligibleCallback on_ineligible_callback,
    TabProcessedCallback on_processed_callback,
    ContextualizedCallback callback,
    std::unique_ptr<ContextualTaskContext> context) {
  // Fail early if the task id was specified but there was no context for the
  // task. This indicates that the task was not available (i.e. was deleted)
  // and no further action is needed.
  if (task_id.has_value() && !context) {
    std::move(callback).Run(session_handle);
    return;
  }

  // If the session handle already exists, track uploads on its query
  // controller.
  scoped_refptr<UploadTracker> upload_tracker;
  if (session_handle && session_handle->GetController()) {
    upload_tracker =
        base::MakeRefCounted<UploadTracker>(session_handle->GetController());
  }

  // Extract URLs from the query text and start upload flows for them.
  if (lens::features::IsLensSendUrlsInComposeboxesEnabled()) {
    std::vector<GURL> extracted_urls = ExtractUrlsFromQuery(query_text);

    // Create the session handle if it did not already exist and there are URLs
    // to upload.
    if (!extracted_urls.empty() && !session_handle) {
      auto* created_handle =
          delegate_->GetOrCreateSessionHandleForQueryContextualizer();
      if (created_handle) {
        session_handle = created_handle->AsWeakPtr();
        if (session_handle->GetController()) {
          upload_tracker = base::MakeRefCounted<UploadTracker>(
              session_handle->GetController());
        }
        created_handle->NotifySessionStarted();
      }
    }

    if (session_handle) {
      for (const GURL& url : extracted_urls) {
        auto context_token = session_handle->CreateContextToken();
        if (upload_tracker) {
          upload_tracker->AddToken(context_token);
        }
        session_handle->StartUrlContextUploadFlow(context_token, url);
      }
    }
  }

  std::vector<TabUpdate> tabs_to_update =
      GetTabsToUpdate(context.get(), tabs_to_recontextualize,
                      tabs_to_force_contextualize, smart_tabs_to_contextualize);

  if (tabs_to_update.empty()) {
    if (upload_tracker) {
      upload_tracker->NotifyUploadsStarted(std::move(callback), session_handle);
    } else if (callback) {
      std::move(callback).Run(session_handle);
    }
    return;
  }

  base::OnceClosure on_all_tabs_fetched;
  if (upload_tracker) {
    on_all_tabs_fetched =
        base::BindOnce(&UploadTracker::NotifyUploadsStarted, upload_tracker,
                       std::move(callback), session_handle);
  } else {
    on_all_tabs_fetched = base::BindOnce(std::move(callback), session_handle);
  }

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      tabs_to_update.size(), std::move(on_all_tabs_fetched));

  for (const TabUpdate& update : tabs_to_update) {
    delegate_->GetPageContext(
        update.id,
        base::BindOnce(
            &QueryContextualizer::OnTabContextualizationFetched,
            weak_factory_.GetWeakPtr(), task_id,
            context ? std::make_unique<ContextualTaskContext>(*context)
                    : nullptr,
            barrier_closure, update.id, update.is_recontextualization,
            update.is_smart_selection, update.is_auto_suggested, session_handle,
            upload_tracker, on_ineligible_callback, on_processed_callback));
  }
}

void QueryContextualizer::OnTabContextualizationFetched(
    const std::optional<base::Uuid>& task_id,
    std::unique_ptr<ContextualTaskContext> context,
    base::RepeatingClosure barrier_closure,
    TabId tab_id,
    bool is_recontextualization,
    bool is_smart_selection,
    bool is_auto_suggested,
    base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
        session_handle,
    scoped_refptr<UploadTracker> upload_tracker,
    PageContextIneligibleCallback on_ineligible_callback,
    TabProcessedCallback on_processed_callback,
    std::unique_ptr<lens::ContextualInputData> page_content_data) {
  if (!page_content_data) {
    on_processed_callback.Run(tab_id);
    barrier_closure.Run();
    return;
  }

  page_content_data->is_implicit_upload =
      is_recontextualization || is_auto_suggested;
  page_content_data->was_smart_tab_selection = is_smart_selection;

  if (GetIsProtectedPageErrorEnabled() &&
      !page_content_data->is_page_context_eligible.value_or(false)) {
    on_ineligible_callback.Run();
    on_processed_callback.Run(tab_id);
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
    on_processed_callback.Run(tab_id);
    barrier_closure.Run();
    return;
  }

  if (!session_handle) {
    on_processed_callback.Run(tab_id);
    barrier_closure.Run();
    return;
  }

  if (!delegate_->IsTabValid(tab_id)) {
    on_processed_callback.Run(tab_id);
    barrier_closure.Run();
    return;
  }

  auto context_token = session_handle->CreateContextToken();
  if (maybe_context_id.has_value()) {
    page_content_data->context_id = maybe_context_id.value();
  }
  if (upload_tracker) {
    upload_tracker->AddToken(context_token);
  }
  session_handle->StartTabContextUploadFlow(
      context_token, std::move(page_content_data),
      delegate_->GetTabViewportEncodingOptionsForQueryContextualizer());

  on_processed_callback.Run(tab_id);
  barrier_closure.Run();
}

std::vector<QueryContextualizer::TabUpdate>
QueryContextualizer::GetTabsToUpdate(
    const ContextualTaskContext* context,
    const std::vector<TabId>& tabs_to_recontextualize,
    const std::vector<TabId>& tabs_to_force_contextualize,
    const std::vector<TabId>& smart_tabs_to_contextualize) {
  std::vector<TabUpdate> tabs_to_update;
  std::set<TabId> added_tabs;

  for (TabId id : tabs_to_force_contextualize) {
    if (!added_tabs.contains(id)) {
      tabs_to_update.push_back({id, /*is_recontextualization=*/false,
                                /*is_smart_selection=*/false,
                                /*is_auto_suggested=*/true});
      added_tabs.insert(id);
    }
  }

  if (context) {
    for (TabId id : tabs_to_recontextualize) {
      if (added_tabs.contains(id)) {
        continue;
      }

      GURL url = delegate_->GetTabUrl(id);
      SessionID session_id = delegate_->GetTabSessionId(id);

      if (GetMatchingAttachment(*context, url, session_id)) {
        tabs_to_update.push_back({id, /*is_recontextualization=*/true,
                                  /*is_smart_selection=*/false});
        added_tabs.insert(id);
      }
    }
  }

  for (TabId id : smart_tabs_to_contextualize) {
    if (!added_tabs.contains(id)) {
      tabs_to_update.push_back(
          {id, /*is_recontextualization=*/false, /*is_smart_selection=*/true});
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

  bool context_changed = viewport_changed;
  if (GetIsWebpageApcComparisonEnabled() &&
      new_data.primary_content_type != lens::MimeType::kPdf) {
    context_changed |= page_content_changed;
  }

  if (!context_changed) {
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
