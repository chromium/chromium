// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_PAGE_CONTEXT_MONITOR_H_
#define CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_PAGE_CONTEXT_MONITOR_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/ai_overlay_dialog_page_handler.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "content/public/browser/web_contents_observer.h"

class BrowserWindowInterface;

namespace ttc {

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
  void DidStopLoading() override;

  std::string GetUrlForHash(const std::string& hash) const;

  const std::optional<optimization_guide::proto::AnnotatedPageContent>&
  last_page_content() const {
    return last_page_content_;
  }

 private:
  void OnActiveTabChanged(BrowserWindowInterface* window);
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

  base::WeakPtrFactory<PageContextMonitor> weak_ptr_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_PAGE_CONTEXT_MONITOR_H_
