// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_DESKTOP_QUERY_CONTEXTUALIZER_DELEGATE_H_
#define CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_DESKTOP_QUERY_CONTEXTUALIZER_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/contextual_tasks/public/query_contextualizer.h"

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

class BrowserWindowInterface;

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace contextual_tasks {

class ContextualTasksContextService;

// A shared implementation of QueryContextualizer::Delegate.
// It uses tabs::TabHandle to interact with tabs and delegates page context
// fetching to TabContextualizationController.
class DesktopQueryContextualizerDelegate
    : public QueryContextualizer::Delegate {
 public:
  using GetSessionHandleCallback = base::RepeatingCallback<
      contextual_search::ContextualSearchSessionHandle*()>;

  using GetViewportEncodingOptionsCallback =
      base::RepeatingCallback<std::optional<lens::ImageEncodingOptions>()>;

  DesktopQueryContextualizerDelegate(
      GetSessionHandleCallback get_session_callback,
      GetViewportEncodingOptionsCallback get_viewport_options_callback,
      ContextualTasksContextService* service,
      BrowserWindowInterface* browser_window_interface);
  ~DesktopQueryContextualizerDelegate() override;

  // QueryContextualizer::Delegate:
  GURL GetTabUrl(QueryContextualizer::TabId id) override;
  SessionID GetTabSessionId(QueryContextualizer::TabId id) override;
  void GetPageContext(
      QueryContextualizer::TabId id,
      base::OnceCallback<void(std::unique_ptr<lens::ContextualInputData>)>
          callback) override;
  bool IsTabValid(QueryContextualizer::TabId id) override;
  std::optional<lens::ImageEncodingOptions>
  GetTabViewportEncodingOptionsForQueryContextualizer() override;
  contextual_search::ContextualSearchSessionHandle*
  GetOrCreateSessionHandleForQueryContextualizer() override;
  void GetRelevantTabsForQuery(
      const std::string& query_text,
      const std::vector<GURL>& attached_context_urls,
      base::OnceCallback<void(std::vector<QueryContextualizer::TabId>)>
          callback) override;

 private:
  tabs::TabInterface* GetTab(QueryContextualizer::TabId id);

  GetSessionHandleCallback get_session_callback_;
  GetViewportEncodingOptionsCallback get_viewport_options_callback_;
  raw_ptr<ContextualTasksContextService> service_;
  base::WeakPtr<BrowserWindowInterface> browser_window_interface_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_DESKTOP_QUERY_CONTEXTUALIZER_DELEGATE_H_
