// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/page_context_monitor.h"

#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/page_content_annotations/page_content_screenshot_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/re2/src/re2/re2.h"

namespace ttc {

const base::TimeDelta kEmptyPageRetryDelay = base::Seconds(2);

using page_content_annotations::PageContentScreenshotServiceFactory;

namespace {

ai_overlay_dialog::mojom::PageContentNodePtr ConvertContentNodeToMojo(
    const optimization_guide::proto::ContentNode& proto_node) {
  auto mojo_node = ai_overlay_dialog::mojom::PageContentNode::New();
  if (proto_node.has_content_attributes()) {
    const auto& attrs = proto_node.content_attributes();
    mojo_node->dom_node_id = attrs.common_ancestor_dom_node_id();

    if (attrs.attribute_type() ==
        optimization_guide::proto::ContentAttributeType::
            CONTENT_ATTRIBUTE_IMAGE) {
      mojo_node->tag_name = "img";
      if (attrs.has_image_data()) {
        mojo_node->text = attrs.image_data().image_caption();
      }
    }

    if (attrs.has_text_data()) {
      mojo_node->text = attrs.text_data().text_content();
    }
    if (attrs.has_anchor_data()) {
      mojo_node->url = GURL(attrs.anchor_data().url());
    }
  }
  for (const auto& child : proto_node.children_nodes()) {
    mojo_node->children.push_back(ConvertContentNodeToMojo(child));
  }
  return mojo_node;
}

std::string FindUrlForDomNodeId(
    const optimization_guide::proto::ContentNode& node,
    int target_id) {
  if (node.has_content_attributes()) {
    const auto& attrs = node.content_attributes();
    if (attrs.has_anchor_data()) {
      int url_hash = static_cast<int>(
          base::PersistentHash(attrs.anchor_data().url()) % 10000);
      if (attrs.common_ancestor_dom_node_id() == target_id ||
          url_hash == target_id) {
        return attrs.anchor_data().url();
      }
    }
  }
  for (const auto& child : node.children_nodes()) {
    std::string url = FindUrlForDomNodeId(child, target_id);
    if (!url.empty()) {
      return url;
    }
  }
  return "";
}

}  // namespace

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
  last_page_content_.reset();
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
  Observe(active_tab ? active_tab->GetContents() : nullptr);
  last_page_content_.reset();

  if (!active_tab) {
    return;
  }

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
    last_page_content_ =
        fetch_result.annotated_page_content_result.value().proto;
    ai_overlay_dialog::mojom::PageContentNodePtr root_mojo_node;
    if (last_page_content_->has_root_node()) {
      root_mojo_node =
          ConvertContentNodeToMojo(last_page_content_->root_node());
    }

    // If the page looks mostly empty, crudely wait a bit and retry in case the
    // load comes before content is shown.
    if (!did_retry_first_fetch_ && !root_mojo_node) {
      did_retry_first_fetch_ = true;
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PageContextMonitor::StartNewFetch,
                         weak_ptr_factory_.GetWeakPtr()),
          kEmptyPageRetryDelay);
    }

    page_handler_->UpdateCurrentPageContext(web_contents()->GetTitle(),
                                            std::move(root_mojo_node));
  }
}

std::string PageContextMonitor::GetUrlForHash(
    const std::string& hash_str) const {
  if (!last_page_content_.has_value()) {
    return "";
  }
  std::string_view hash_sv(hash_str);
  while (!hash_sv.empty() &&
         (hash_sv.front() == '{' || hash_sv.front() == '#')) {
    hash_sv.remove_prefix(1);
  }
  while (!hash_sv.empty() && hash_sv.back() == '}') {
    hash_sv.remove_suffix(1);
  }
  int target_hash;
  if (!base::StringToInt(hash_sv, &target_hash)) {
    return "";
  }
  return FindUrlForDomNodeId(last_page_content_->root_node(), target_hash);
}

}  // namespace ttc
