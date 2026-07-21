// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTEXT_ELIGIBILITY_OBSERVER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTEXT_ELIGIBILITY_OBSERVER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_api.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content_metadata.mojom.h"

namespace optimization_guide {

enum class PageContextEligibilityStatus {
  // Initial state. Persists while the eligibility API is loading
  // asynchronously.
  kUnknown,
  // The page context is eligible.
  kEligible,
  // The page context is not eligible.
  kNotEligible,
};

class PageContentMetadataObserver;
class PageContextEligibility;

// Tracks the page context eligibility for a WebContents.
class PageContextEligibilityObserver : public content::WebContentsObserver {
 public:
  // Creates a PageContextEligibilityObserver instance. Returns null if loading
  // the library failed or if the required APIs are not all available. The
  // caller must ensure that this observer does not outlive the `web_contents`.
  static std::unique_ptr<PageContextEligibilityObserver> Create(
      content::WebContents* web_contents,
      std::string account,
      base::RepeatingCallback<void(bool)> callback);

  ~PageContextEligibilityObserver() override;

  // Returns whether the current page context is eligible.
  PageContextEligibilityStatus IsPageContextEligible() const;

 private:
  PageContextEligibilityObserver(content::WebContents* web_contents,
                                 std::string account,
                                 base::RepeatingCallback<void(bool)> callback);

  bool ComputePageContextEligibility();

  friend class PageContextEligibilityObserverTest;

  void OnMetaTagsChanged(blink::mojom::PageMetadataPtr metadata);

  void CheckEligibilityAndNotify();

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void UpdateObserver();

  void StartApiLoading();
  void OnApiLoaded(const PageContextEligibility* api);

  const std::string account_;
  base::RepeatingCallback<void(bool)> callback_;

  raw_ptr<const PageContextEligibility> api_holder_ = nullptr;
  bool is_api_loaded_ = false;

  std::unique_ptr<PageContentMetadataObserver> meta_tags_observer_;
  std::vector<std::string> observed_meta_tag_names_;
  std::vector<FrameMetadata> current_metadata_;
  bool is_permanently_ineligible_ = false;

  PageContextEligibilityStatus last_eligibility_ =
      PageContextEligibilityStatus::kUnknown;

  base::WeakPtrFactory<PageContextEligibilityObserver> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTEXT_ELIGIBILITY_OBSERVER_H_
