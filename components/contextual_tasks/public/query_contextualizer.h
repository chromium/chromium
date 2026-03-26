// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_QUERY_CONTEXTUALIZER_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_QUERY_CONTEXTUALIZER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "components/lens/contextual_input.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace contextual_tasks {
class ContextualTasksService;
struct ContextualTaskContext;
struct UrlAttachment;
}  // namespace contextual_tasks

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

namespace contextual_tasks {

// Helper class to orchestrate contextualization of tabs right before submitting
// a query. It is strictly an on-submit orchestrator helper, and is not the only
// way context can be added to the session.
// This class is platform-agnostic, delegating UI-specific lookups (URL,
// SessionID, context fetch, and upload) to its Delegate.
// TODO(crbug.com/493620133): Migrate test coverage from
// `contextual_tasks_composebox_handler_unittest` to a separate unit test.
class QueryContextualizer {
 public:
  // Type-erased identifier for a tab. Allows for platform-agnostic
  // implementation.
  using TabId = int32_t;

  struct TabUpdate {
    TabId id = 0;
    bool is_recontextualization = false;
  };

  // Delegate interface that allows clients to provide platform-specific
  // information and trigger platform-specific actions.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns the current URL of the tab.
    virtual GURL GetTabUrl(TabId id) = 0;

    // Returns the SessionID of the tab.
    virtual SessionID GetTabSessionId(TabId id) = 0;

    // Triggers an asynchronous fetch of the page context for the tab.
    // `callback` must be run with the retrieved data (or nullptr on failure).
    virtual void GetPageContext(
        TabId id,
        base::OnceCallback<void(std::unique_ptr<lens::ContextualInputData>)>
            callback) = 0;

    // Triggers an upload of the tab context with the retrieved data.
    // `callback` must be run with success/failure status.
    virtual void UploadTabContextWithData(
        TabId id,
        std::optional<int64_t> context_id,
        std::unique_ptr<lens::ContextualInputData> data,
        base::OnceCallback<void(bool)> callback) = 0;

    // Called when the page context is ineligible.
    virtual void OnPageContextIneligible() = 0;

    // Called when contextualization for a tab has been processed (either
    // uploaded or skipped), to allow state cleanup.
    virtual void OnTabProcessedForQueryContextualization(TabId id) = 0;
  };

  QueryContextualizer(ContextualTasksService* service, Delegate* delegate);
  ~QueryContextualizer();

  QueryContextualizer(const QueryContextualizer&) = delete;
  QueryContextualizer& operator=(const QueryContextualizer&) = delete;

  // Starts the contextualization flow for the given task and tabs.
  // `task_id` is the ID of the active contextual task to contextualize for,
  // used to check for previous context to determine if a tab should be
  // re-uploaded. If empty, recontextualization will always be run for all tabs.
  // `tabs_to_recontextualize` are tabs that will only be processed if they
  // are already part of the task's context, and will be re-uploaded only if
  // their content has changed since the previous upload (e.g., the active tab).
  // `tabs_to_force_contextualize` are tabs that will be processed
  // unconditionally (added to the context if missing), and will also run change
  // checks before re-uploading if they are already present (e.g.,
  // auto-suggested chips). `callback` is invoked when processing for all tabs
  // is complete.
  void Contextualize(
      const std::optional<base::Uuid>& task_id,
      const std::string& query_text,
      const std::vector<TabId>& tabs_to_recontextualize,
      const std::vector<TabId>& tabs_to_force_contextualize,
      contextual_search::ContextualSearchSessionHandle* session_handle,
      base::OnceClosure callback);

 private:
  void OnContextRetrieved(
      const std::optional<base::Uuid>& task_id,
      const std::string& query_text,
      const std::vector<TabId>& tabs_to_recontextualize,
      const std::vector<TabId>& tabs_to_force_contextualize,
      base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
          session_handle,
      base::OnceClosure callback,
      std::unique_ptr<ContextualTaskContext> context);

  void OnTabContextualizationFetched(
      const std::optional<base::Uuid>& task_id,
      std::unique_ptr<ContextualTaskContext> context,
      base::RepeatingClosure barrier_closure,
      TabId tab_id,
      bool is_recontextualization,
      base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
          session_handle,
      std::unique_ptr<lens::ContextualInputData> page_content_data);

  std::vector<TabUpdate> GetTabsToUpdate(
      const ContextualTaskContext* context,
      const std::vector<TabId>& tabs_to_recontextualize,
      const std::vector<TabId>& tabs_to_force_contextualize);

  std::optional<int64_t> GetContextIdForTab(
      const ContextualTaskContext& context,
      const lens::ContextualInputData& page_content_data,
      base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
          session_handle);

  // Checks if the context has changed since the previous upload. If only the
  // viewport has changed, it modifies `page_content_data` to strip out the
  // unchanged page contents so that only the new viewport is uploaded.
  // Returns true if the context is completely unchanged (no upload needed),
  // and false if there are changes (upload needed).
  bool CheckIfContextChangedAndPrepareUploadData(
      std::optional<int64_t> context_id,
      lens::ContextualInputData& page_content_data,
      base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
          session_handle);

  const UrlAttachment* GetMatchingAttachment(
      const ContextualTaskContext& context,
      const GURL& url,
      SessionID session_id);

  const raw_ptr<ContextualTasksService> service_;
  const raw_ptr<Delegate> delegate_;

  base::WeakPtrFactory<QueryContextualizer> weak_factory_{this};
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_QUERY_CONTEXTUALIZER_H_
