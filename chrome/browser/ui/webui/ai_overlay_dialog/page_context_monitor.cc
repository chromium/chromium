// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/page_context_monitor.h"

#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/page_content_annotations/page_content_screenshot_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/markdown_builder.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/re2/src/re2/re2.h"

namespace ttc {

static constexpr int kTruncateThresholdBytes = 30000;
static constexpr int kEmptyPageThreshold = 200;
const base::TimeDelta kEmptyPageRetryDelay = base::Seconds(2);

using page_content_annotations::PageContentScreenshotServiceFactory;

PageContextMonitor::PageContextMonitor(BrowserWindowInterface& window,
                                       AiOverlayDialogPageHandler& page_handler)
    : window_(window), page_handler_(page_handler) {
  active_tab_subscription_ =
      window.RegisterActiveTabDidChange(base::BindRepeating(
          &PageContextMonitor::OnActiveTabChanged, base::Unretained(this)));
  OnActiveTabChanged(&window);
}

PageContextMonitor::~PageContextMonitor() = default;

void PageContextMonitor::PrimaryPageChanged(content::Page& page) {
  page_handler_->DidChangePage(web_contents()->GetLastCommittedURL(),
                               web_contents()->GetTitle(), std::nullopt);
  did_retry_first_fetch_ = false;
  StartNewFetch();
}

void PageContextMonitor::DidStopLoading() {
  if (fetch_waiting_on_load_) {
    StartNewFetch();
  }
}

void PageContextMonitor::OnActiveTabChanged(BrowserWindowInterface* window) {
  CHECK_EQ(window, &window_.get());

  tabs::TabInterface* active_tab = window_->GetActiveTabInterface();
  if (!active_tab) {
    return;
  }

  Observe(active_tab->GetContents());
  page_handler_->DidChangePage(web_contents()->GetLastCommittedURL(),
                               web_contents()->GetTitle(), std::nullopt);
  StartNewFetch();
}

void PageContextMonitor::StartNewFetch() {
  fetch_waiting_on_load_ = false;
  fetcher_.reset();

  content::WebContents* contents = web_contents();
  if (!contents) {
    return;
  }

  if (contents->IsLoading()) {
    fetch_waiting_on_load_ = true;
    return;
  }

  fetcher_ = std::make_unique<page_content_annotations::PageContextFetcher>(
      base::BindRepeating([](content::BrowserContext* context) {
        return PageContentScreenshotServiceFactory::GetForProfile(
            Profile::FromBrowserContext(context));
      }),
      /*progress_listener=*/nullptr);

  page_content_annotations::FetchPageContextOptions options;
  options.annotated_page_content_options =
      optimization_guide::DefaultAIPageContentOptions(
          /* on_critical_path =*/true);
  options.annotated_page_content_options->max_meta_elements = 32;

  fetcher_->FetchStart(*contents, options,
                       base::BindOnce(&PageContextMonitor::OnFetchComplete,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void PageContextMonitor::OnFetchComplete(
    page_content_annotations::FetchPageContextResultCallbackArg result) {
  if (!result.has_value()) {
    return;
  }

  const page_content_annotations::FetchPageContextResult& fetch_result =
      **result;

  if (fetch_result.annotated_page_content_result.has_value()) {
    MarkdownBuilder markdown_builder(
        fetch_result.annotated_page_content_result.value().proto);
    std::string markdown_content = markdown_builder.Build();

    // TODO(bokan): More sophisticated truncation.
    std::string markdown_content_truncated =
        markdown_content.substr(0, kTruncateThresholdBytes);

    // If the page looks mostly empty, crudely wait a bit and retry in case the
    // load comes before content is shown.
    if (!did_retry_first_fetch_ &&
        markdown_content_truncated.length() < kEmptyPageThreshold) {
      did_retry_first_fetch_ = true;
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PageContextMonitor::StartNewFetch,
                         weak_ptr_factory_.GetWeakPtr()),
          kEmptyPageRetryDelay);
    }

    page_handler_->UpdateCurrentPageContext(web_contents()->GetTitle(),
                                            markdown_content_truncated);
  }
}

}  // namespace ttc
