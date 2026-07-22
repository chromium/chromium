// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_context_eligibility_observer.h"

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/content/browser/page_content_metadata_observer.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content_metadata.mojom.h"

namespace optimization_guide {

// static
std::unique_ptr<PageContextEligibilityObserver>
PageContextEligibilityObserver::Create(content::WebContents* web_contents,
                                       std::string account,
                                       EligibilityChangedCallback callback) {
  if (!web_contents) {
    return nullptr;
  }
  return base::WrapUnique(new PageContextEligibilityObserver(
      web_contents, std::move(account), std::move(callback)));
}

PageContextEligibilityObserver::PageContextEligibilityObserver(
    content::WebContents* web_contents,
    std::string account,
    EligibilityChangedCallback callback)
    : content::WebContentsObserver(web_contents),
      account_(std::move(account)),
      callback_(std::move(callback)) {
  StartApiLoading();
}

PageContextEligibilityObserver::~PageContextEligibilityObserver() = default;

bool PageContextEligibilityObserver::CanComputePageContextEligibility() const {
  if (!is_api_loaded_) {
    return false;
  }
  if (!observed_meta_tag_names_.empty() &&
      !received_meta_tags_for_current_page_) {
    return false;
  }
  return true;
}

void PageContextEligibilityObserver::CheckEligibilityAndNotify() {
  if (!CanComputePageContextEligibility()) {
    return;
  }
  bool is_eligible = ComputePageContextEligibility();
  PageContextEligibilityStatus current_eligibility =
      is_eligible ? PageContextEligibilityStatus::kEligible
                  : PageContextEligibilityStatus::kNotEligible;

  bool is_initial_evaluation =
      (last_eligibility_ == PageContextEligibilityStatus::kUnknown);
  if (is_initial_evaluation || last_eligibility_ != current_eligibility) {
    last_eligibility_ = current_eligibility;
    if (callback_) {
      callback_.Run(current_eligibility);
    }
  }
}

PageContextEligibilityStatus
PageContextEligibilityObserver::IsPageContextEligible() const {
  return last_eligibility_;
}

bool PageContextEligibilityObserver::ComputePageContextEligibility() {
  if (is_permanently_ineligible_ || !web_contents()) {
    return false;
  }

  const GURL& url = web_contents()->GetLastCommittedURL();
  std::string host = std::string(url.host());
  std::string path = std::string(url.path());

  return IsPageContextEligibleWithAccount(host, path, account_,
                                          current_metadata_, api_holder_);
}

void PageContextEligibilityObserver::ResetToUnknown() {
  meta_tags_observer_.reset();
  current_metadata_.clear();
  received_meta_tags_for_current_page_ = false;
  if (last_eligibility_ != PageContextEligibilityStatus::kUnknown) {
    last_eligibility_ = PageContextEligibilityStatus::kUnknown;
    if (callback_) {
      callback_.Run(PageContextEligibilityStatus::kUnknown);
    }
  }
}

// When the primary page changes, immediately transition eligibility to
// `kUnknown` to clear state from the previous page. If the new page's
// eligibility is conditional on `<meta>` tags, evaluation is asynchronous
// while waiting for IPC `OnMetaTagsChanged` from the renderer.
void PageContextEligibilityObserver::PrimaryPageChanged(content::Page& page) {
  ResetToUnknown();
  UpdateObserver();
}

// We only update eligibility on `RenderFrameCreated` and `RenderFrameDeleted`
// for subframes (`render_frame_host->GetParent() != nullptr`) belonging to the
// primary page.
//
// For main frames, when a navigation starts that requires a new RenderFrameHost
// (e.g. with RenderDocument enabled), updating eligibility on frame creation
// is redundant and premature because the tab's eligibility does not change
// until the new RenderFrameHost commits (handled via `PrimaryPageChanged`).
//
// For subframes, `RenderFrameCreated` handles new child frames attaching to
// the primary page, and `RenderFrameDeleted` is essential to recompute
// eligibility when a subframe is removed.
void PageContextEligibilityObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->GetParent() &&
      &render_frame_host->GetPage() == &web_contents()->GetPrimaryPage()) {
    UpdateObserver();
  }
}

void PageContextEligibilityObserver::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->GetParent() &&
      &render_frame_host->GetPage() == &web_contents()->GetPrimaryPage()) {
    UpdateObserver();
  }
}

void PageContextEligibilityObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();

  // PrimaryPageChanged handles main frame navigations. We only care about
  // subframes here.
  if (!render_frame_host->GetParent()) {
    return;
  }

  if (&render_frame_host->GetPage() == &web_contents()->GetPrimaryPage()) {
    UpdateObserver();
  }
}

void PageContextEligibilityObserver::WebContentsDestroyed() {
  ResetToUnknown();
}

void PageContextEligibilityObserver::UpdateObserver() {
  if (!is_api_loaded_ || !web_contents()) {
    return;
  }
  std::vector<optimization_guide::FrameUrl> current_frames;
  if (web_contents()->GetPrimaryMainFrame()) {
    web_contents()->GetPrimaryMainFrame()->ForEachRenderFrameHost(
        [&current_frames](content::RenderFrameHost* rfh) {
          if (!rfh->IsRenderFrameLive()) {
            return;
          }
          GURL frame_url = GetURLForFrameMetadata(
              rfh->GetLastCommittedURL(), rfh->GetLastCommittedOrigin());
          if (frame_url.is_empty()) {
            return;
          }
          current_frames.emplace_back(frame_url.host(), frame_url.path());
        });
  }

  PageEligibilityResult result =
      CheckPageEligibility(current_frames, api_holder_);
  is_permanently_ineligible_ = result.status == PageEligibility::kIneligible;

  std::vector<std::string> names;
  if (result.status == PageEligibility::kConditionalOnMetaTags &&
      result.meta_tag_names_affecting_eligibility.size > 0 &&
      result.meta_tag_names_affecting_eligibility.data != nullptr) {
    // SAFETY: The API guarantees that `data` points to an array of length
    // `size`.
    auto tags = UNSAFE_BUFFERS(
        base::span(result.meta_tag_names_affecting_eligibility.data,
                   result.meta_tag_names_affecting_eligibility.size));
    for (const auto& tag : tags) {
      names.emplace_back(tag);
    }
  }

  if (names != observed_meta_tag_names_) {
    meta_tags_observer_.reset();
    observed_meta_tag_names_ = names;
    current_metadata_.clear();

    if (!names.empty()) {
      meta_tags_observer_ = std::make_unique<PageContentMetadataObserver>(
          web_contents(), names,
          base::BindRepeating(
              &PageContextEligibilityObserver::OnMetaTagsChanged,
              base::Unretained(this)));
    }
  }

  CheckEligibilityAndNotify();
}

void PageContextEligibilityObserver::OnMetaTagsChanged(
    blink::mojom::PageMetadataPtr metadata) {
  received_meta_tags_for_current_page_ = true;
  current_metadata_.clear();
  if (!metadata) {
    CheckEligibilityAndNotify();
    return;
  }

  for (const auto& frame_metadata_mojom : metadata->frame_metadata) {
    std::vector<MetaTag> meta_tags;
    meta_tags.reserve(frame_metadata_mojom->meta_tags.size());
    for (const auto& tag : frame_metadata_mojom->meta_tags) {
      meta_tags.emplace_back(tag->name, tag->content);
    }
    current_metadata_.emplace_back(
        std::string(frame_metadata_mojom->url.host()),
        std::string(frame_metadata_mojom->url.path()), std::move(meta_tags));
  }

  CheckEligibilityAndNotify();
}

void PageContextEligibilityObserver::StartApiLoading() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&PageContextEligibility::Get),
      base::BindOnce(&PageContextEligibilityObserver::OnApiLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PageContextEligibilityObserver::OnApiLoaded(
    const PageContextEligibility* api) {
  api_holder_ = api;
  is_api_loaded_ = true;
  UpdateObserver();
}

}  // namespace optimization_guide
