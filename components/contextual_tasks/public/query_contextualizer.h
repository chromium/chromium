// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_QUERY_CONTEXTUALIZER_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_QUERY_CONTEXTUALIZER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_bitmap_processing.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace contextual_tasks {
class ContextualTasksService;
struct ContextualTaskContext;
struct UrlAttachment;
class UploadTracker;
}  // namespace contextual_tasks

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

namespace contextual_tasks {

// Represents a single turn in a thread.
struct ThreadTurn {
  ThreadTurn();
  ThreadTurn(const ThreadTurn&);
  ThreadTurn& operator=(const ThreadTurn&);
  ~ThreadTurn();

  // User query for this turn.
  std::string query;
};

// Represents a conversation thread, including current and previous turns.
struct ConversationThread {
  ConversationThread();
  ConversationThread(const ConversationThread&);
  ConversationThread& operator=(const ConversationThread&);
  ~ConversationThread();

  // The query from the current turn.
  std::string query;

  // Previous turns in the thread, in chronological order (oldest first).
  // The first element in this vector is the first turn in the thread.
  std::vector<ThreadTurn> previous_turns;

  // Titles of shared (attached as context) tabs.
  // These are union of tabs shared across all previous turns.
  std::vector<std::string> shared_tab_titles;
};

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
    bool is_smart_selection = false;
    bool is_auto_suggested = false;
    bool is_contextual_searchbox_first_turn = false;
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

    // Returns whether the tab is still valid.
    virtual bool IsTabValid(TabId id) = 0;

    // Returns the image encoding options to use for tab contextualization.
    virtual std::optional<lens::ImageEncodingOptions>
    GetTabViewportEncodingOptionsForQueryContextualizer() = 0;


    // Returns the session handle for context upload, creating it if necessary.
    // If it cannot create one, returning nullptr is fine.
    virtual contextual_search::ContextualSearchSessionHandle*
    GetOrCreateSessionHandleForQueryContextualizer() = 0;

    // Fetches relevant tabs for the given query.
    virtual void GetRelevantTabsForQuery(
        const std::string& query_text,
        const std::vector<GURL>& attached_context_urls,
        base::OnceCallback<void(std::vector<TabId>)> callback) = 0;
  };

  // `service` can be nullptr if task-scoped context decoration is not needed
  // (e.g. standard Omnibox unimodal search).
  QueryContextualizer(ContextualTasksService* service, Delegate* delegate);
  virtual ~QueryContextualizer();

  QueryContextualizer(const QueryContextualizer&) = delete;
  QueryContextualizer& operator=(const QueryContextualizer&) = delete;

  using ContextualizedCallback = base::OnceCallback<void(
      base::WeakPtr<contextual_search::ContextualSearchSessionHandle>)>;

  using PageContextIneligibleCallback = base::RepeatingClosure;
  using TabProcessedCallback = base::RepeatingCallback<void(TabId)>;

  struct ContextualizeParams {
    ContextualizeParams();
    ~ContextualizeParams();
    ContextualizeParams(const ContextualizeParams&) = delete;
    ContextualizeParams& operator=(const ContextualizeParams&) = delete;
    ContextualizeParams(ContextualizeParams&&);
    ContextualizeParams& operator=(ContextualizeParams&&);

    // The task ID associated with the query, if any.
    std::optional<base::Uuid> task_id;
    // The query text to contextualize.
    std::string query_text;
    // Tabs that are already associated with the task context and should be
    // updated (e.g. if the user navigated to a new URL in the same tab, or to
    // refresh the context).
    std::vector<TabId> tabs_to_recontextualize;
    // Tabs that are being contextualized as a result of the user clicking an
    // auto-suggest chip.
    std::vector<TabId> auto_suggested_chip_tabs;
    // Tabs that are being contextualized for the first turn of a query from
    // the contextual searchbox.
    std::vector<TabId> tabs_for_contextual_searchbox_first_turn;
    // Optional callback when a page context is ineligible.
    PageContextIneligibleCallback on_ineligible_callback;
    // Optional callback when a tab is processed.
    TabProcessedCallback on_processed_callback;
    // Optional callback invoked immediately when context uploads have been
    // initiated. Can be used for early URL navigation while uploads continue in
    // the background.
    ContextualizedCallback on_uploads_started_callback;
    // Called when contextualization is complete.
    ContextualizedCallback complete_callback;
    bool enable_smart_tab_selection = false;
  };

  // Starts the contextualization flow for the given parameters.
  virtual void Contextualize(ContextualizeParams params);

  // Extracts URLs from the query text.
  static std::vector<std::string> ExtractUrlsFromQuery(
      const std::string& query_text);

 private:
  void OnRelevantTabsFetched(
      ContextualizeParams params,
      base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
          session_handle,
      std::vector<TabId> smart_tabs);

  void OnContextRetrieved(
      ContextualizeParams params,
      const std::vector<TabId>& smart_tabs_to_contextualize,
      base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
          session_handle,
      std::unique_ptr<ContextualTaskContext> context);

  void OnTabContextualizationFetched(
      const std::optional<base::Uuid>& task_id,
      std::unique_ptr<ContextualTaskContext> context,
      base::RepeatingClosure barrier_closure,
      TabUpdate tab_update,
      base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
          session_handle,
      scoped_refptr<UploadTracker> upload_tracker,
      PageContextIneligibleCallback on_ineligible_callback,
      TabProcessedCallback on_processed_callback,
      std::unique_ptr<lens::ContextualInputData> page_content_data);

  void FinishTabProcessing(const TabProcessedCallback& on_processed_callback,
                           TabId tab_id,
                           base::RepeatingClosure barrier_closure);

  std::vector<TabUpdate> GetTabsToUpdate(
      const ContextualTaskContext* context,
      const std::vector<TabId>& tabs_to_recontextualize,
      const std::vector<TabId>& auto_suggested_chip_tabs,
      const std::vector<TabId>& tabs_for_contextual_searchbox_first_turn,
      const std::vector<TabId>& smart_tabs_to_contextualize);

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
