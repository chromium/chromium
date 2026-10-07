// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_PAGE_CONTEXT_MONITOR_H_
#define CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_PAGE_CONTEXT_MONITOR_H_

#include <optional>
#include <string_view>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/ai_overlay_dialog_page_handler.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"

class BrowserWindowInterface;

namespace ttc {

struct CachedWebMcpTool {
  CachedWebMcpTool();
  CachedWebMcpTool(CachedWebMcpTool&&);
  CachedWebMcpTool& operator=(CachedWebMcpTool&&);
  ~CachedWebMcpTool();

  std::string name;
  std::string description;
  base::Value input_schema;
  bool read_only = false;
  bool consequential = true;
};

// Responsible for monitors for changes in the given window's active tab. Will
// signal the page_handler whenever the tab changes and schedules a fetch page
// context.
class PageContextMonitor : public content::WebContentsObserver {
 public:
  PageContextMonitor(BrowserWindowInterface& window,
                     AiOverlayDialogPageHandler& page_handler);
  ~PageContextMonitor() override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidStopLoading() override;

  std::string GetUrlForHash(const std::string& hash) const;

  std::optional<int32_t> ResolveImageDomNodeId(
      std::string_view document_identifier,
      int32_t dom_node_id) const;

  const std::optional<optimization_guide::proto::AnnotatedPageContent>&
  last_page_content() const {
    return last_page_content_;
  }

  const base::UnguessableToken& active_document_id() const {
    return active_document_id_;
  }
  const CachedWebMcpTool* GetWebMcpTool(const std::string& name) const;

 private:
  void OnActiveTabChanged(BrowserWindowInterface* window);
  void ResetPageContextState();
  void StartNewFetch();
  void OnFetchComplete(
      page_content_annotations::FetchPageContextResultCallbackArg result);

  const base::raw_ref<BrowserWindowInterface> window_;
  const base::raw_ref<AiOverlayDialogPageHandler> page_handler_;

  base::CallbackListSubscription active_tab_subscription_;

  bool fetch_waiting_on_load_ = false;
  bool did_retry_first_fetch_ = false;

  std::unique_ptr<page_content_annotations::PageContextFetcher> fetcher_;

  std::optional<optimization_guide::proto::AnnotatedPageContent>
      last_page_content_;
  base::UnguessableToken active_document_id_;
  base::flat_map<std::string, CachedWebMcpTool> webmcp_tools_map_;

  base::WeakPtrFactory<PageContextMonitor> weak_ptr_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_PAGE_CONTEXT_MONITOR_H_
