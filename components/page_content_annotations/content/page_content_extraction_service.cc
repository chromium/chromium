// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/page_content_extraction_service.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/page_content_annotations/content/annotate_page_content_request.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_cache.h"
#include "components/page_content_annotations/core/page_content_cache_handler.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "components/page_content_annotations/core/web_state_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"

namespace page_content_annotations {

namespace {

// LINT.IfChange(EnablementSource)
enum class EnablementSource {
  kNone = 0,
  kFeatureFlag = 1,
  kObserverPresent = 2,
  kBoth = 3,
  kMaxValue = kBoth,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/optimization/enums.xml:PageContentExtractionEnablementSource)

WebStateWrapper ToWebStateWrapper(content::WebContents* web_contents) {
  return WebStateWrapper(
      web_contents->GetBrowserContext()->IsOffTheRecord(),
      web_contents->GetLastCommittedURL(),
      web_contents->GetController().GetLastCommittedEntry()->GetTimestamp(),
      web_contents->GetVisibility() == content::Visibility::VISIBLE
          ? PageContentVisibility::kVisible
          : PageContentVisibility::kHidden);
}

optimization_guide::proto::PageContext ToPageContext(
    const optimization_guide::proto::AnnotatedPageContent& apc,
    content::WebContents* web_contents,
    const std::vector<uint8_t>& screenshot_data) {
  optimization_guide::proto::PageContext page_context;
  *page_context.mutable_annotated_page_content() = apc;
  page_context.set_url(web_contents->GetLastCommittedURL().spec());
  page_context.set_title(base::UTF16ToUTF8(web_contents->GetTitle()));
  if (!screenshot_data.empty()) {
    page_context.set_tab_screenshot(base::Base64Encode(screenshot_data));
  }
  return page_context;
}

// Returns whether the page content cache is enabled.
bool IsPageContentCacheEnabled(feature_engagement::Tracker* tracker) {
  bool enabled = base::FeatureList::IsEnabled(features::kPageContentCache);

#if BUILDFLAG(IS_ANDROID)
  if (enabled && features::kPageContentCacheUseUserEngagement.Get()) {
    if (!tracker) {
      return false;
    }
    // If user engagement is required, and user has not engaged with the
    // feature. Turn off the feature. This is currently only used on Android.
    return tracker->WouldTriggerHelpUI(
        feature_engagement::kIPHFuseboxAttachmentFeature);
  }
#endif  // BUILDFLAG(IS_ANDROID)

  return enabled;
}

// Creates the PageContentCacheHandler if the cache is enabled.
std::unique_ptr<PageContentCacheHandler> CreatePageContentCacheHandler(
    bool is_page_content_cache_enabled,
    os_crypt_async::OSCryptAsync* os_crypt_async,
    const base::FilePath& profile_path) {
  if (!is_page_content_cache_enabled) {
    return nullptr;
  }
  return std::make_unique<PageContentCacheHandler>(
      os_crypt_async, profile_path,
      base::Days(features::kPageContentCacheMaxCacheAgeInDays.Get()));
}

}  // namespace

bool IsPageContentValid(const PageContent& content) {
  return std::visit(
      [](const auto& ref_counted_ptr) { return ref_counted_ptr != nullptr; },
      content);
}

bool IsAnnotatedPageContentPtr(const PageContent& content) {
  return std::holds_alternative<RefCountedAnnotatedPageContentPtr>(content);
}

bool IsPDFTextPtr(const PageContent& content) {
  return std::holds_alternative<RefCountedPDFTextPtr>(content);
}

RefCountedAnnotatedPageContentPtr GetAnnotatedPageContentPtrFromPageContent(
    const PageContent& content) {
  if (const auto* ptr =
          std::get_if<RefCountedAnnotatedPageContentPtr>(&content)) {
    return *ptr;
  }
  return nullptr;
}

RefCountedAnnotatedPageContentPtr GetAnnotatedPageContentPtrFromPageContent(
    PageContent&& content) {
  if (auto* ptr = std::get_if<RefCountedAnnotatedPageContentPtr>(&content)) {
    return std::move(*ptr);
  }
  return nullptr;
}

RefCountedPDFTextPtr GetPDFTextPtrFromPageContent(const PageContent& content) {
  if (const auto* ptr = std::get_if<RefCountedPDFTextPtr>(&content)) {
    return *ptr;
  }
  return nullptr;
}

RefCountedPDFTextPtr GetPDFTextPtrFromPageContent(PageContent&& content) {
  if (auto* ptr = std::get_if<RefCountedPDFTextPtr>(&content)) {
    return std::move(*ptr);
  }
  return nullptr;
}

PageContentExtractionService::PageContentExtractionService(
    os_crypt_async::OSCryptAsync* os_crypt_async,
    const base::FilePath& profile_path,
    feature_engagement::Tracker* tracker)
    : is_page_content_cache_enabled_(IsPageContentCacheEnabled(tracker)),
      page_content_cache_handler_(
          CreatePageContentCacheHandler(is_page_content_cache_enabled_,
                                        os_crypt_async,
                                        profile_path)) {}

PageContentExtractionService::~PageContentExtractionService() {
  ClearAllUserData();
}

void PageContentExtractionService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PageContentExtractionService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

PageContentExtractionEnablementReason
PageContentExtractionService::GetPageContentExtractionEnablementReason(
    bool is_on_demand) const {
  if (base::FeatureList::IsEnabled(page_content_annotations::features::
                                       kAnnotatedPageContentExtraction)) {
    return PageContentExtractionEnablementReason::
        kAutomaticExtractionFeatureEnabled;
  }
  if (!observers_.empty()) {
    return PageContentExtractionEnablementReason::kObserverRegistered;
  }
  if (is_on_demand &&
      base::FeatureList::IsEnabled(
          features::kPageContentExtractionAllowOnDemandWithoutObservers)) {
    return PageContentExtractionEnablementReason::kBypassedObservers;
  }
  return PageContentExtractionEnablementReason::kDisabled;
}

bool PageContentExtractionService::ShouldEnablePageContentExtraction(
    bool is_on_demand) const {
  return GetPageContentExtractionEnablementReason(is_on_demand) !=
         PageContentExtractionEnablementReason::kDisabled;
}

void PageContentExtractionService::OnPageContentExtracted(
    content::Page& page,
    PageContent page_content,
    const std::vector<uint8_t>& screenshot_data,
    std::optional<int> tab_id) {
  for (auto& observer : observers_) {
    observer.OnPageContentExtracted(page, page_content);
  }

  if (!is_page_content_cache_enabled_) {
    return;
  }

  // Note: Unlike APC result, PDF text result is not stored to the cache. The
  // below cache handling logic does not apply to it.
  // TODO(b/487632737): Investigate the support for on-demand PDF text
  // extraction, which may require `page_content_cache_handler_` to interact
  // with the PDF text result.
  RefCountedAnnotatedPageContentPtr annotated_page_content_ptr =
      GetAnnotatedPageContentPtrFromPageContent(page_content);
  if (!annotated_page_content_ptr) {
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&page.GetMainDocument());
  if (!web_contents) {
    return;
  }

  page_content_cache_handler_->ProcessPageContentExtraction(
      tab_id, ToWebStateWrapper(web_contents),
      ToPageContext(annotated_page_content_ptr->data, web_contents,
                    screenshot_data),
      base::Time::Now());
}

std::optional<ExtractedPageContentResult>
PageContentExtractionService::GetExtractedPageContentAndEligibilityForPage(
    content::Page& page) {
  AnnotatedPageContentRequest* request =
      GetAnnotatedPageContentRequestFromPage(page);
  return request ? request->GetCachedContentAndEligibility() : std::nullopt;
}

void PageContentExtractionService::
    RefreshExtractedPageContentAndEligibilityForPage(
        content::Page& page,
        GetExtractedPageContentAndEligibilityCallback callback) {
  AnnotatedPageContentRequest* request =
      GetAnnotatedPageContentRequestFromPage(page);
  if (request) {
    request->RefreshExtractedPageContentAndEligibilityForPage(
        std::move(callback));
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

void PageContentExtractionService::
    GetExtractedPageContentAndEligibilityForPageAsync(
        content::Page& page,
        GetExtractedPageContentAndEligibilityCallback callback,
        bool trigger_if_not_cached) {
  AnnotatedPageContentRequest* request =
      GetAnnotatedPageContentRequestFromPage(page);
  if (!request) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (trigger_if_not_cached) {
    request->GetContentAndEligibilityAsync(std::move(callback));
  } else {
    request->GetCachedContentAndEligibilityAsync(std::move(callback));
  }
}

std::optional<bool>
PageContentExtractionService::GetServerUploadEligibilityForPage(
    content::Page& page) {
  AnnotatedPageContentRequest* request =
      GetAnnotatedPageContentRequestFromPage(page);
  return request ? request->GetServerUploadEligibility() : std::nullopt;
}

void PageContentExtractionService::GetServerUploadEligibilityForPageAsync(
    content::Page& page,
    GetServerUploadEligibilityCallback callback) {
  AnnotatedPageContentRequest* request =
      GetAnnotatedPageContentRequestFromPage(page);
  if (request) {
    request->GetServerUploadEligibilityAsync(std::move(callback));
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

void PageContentExtractionService::OnTabClosed(int64_t tab_id) {
  if (is_page_content_cache_enabled_) {
    page_content_cache_handler_->OnTabClosed(tab_id);
  }
}

void PageContentExtractionService::OnTabCloseUndone(int64_t tab_id) {
  if (is_page_content_cache_enabled_) {
    page_content_cache_handler_->OnTabCloseUndone(tab_id);
  }
}

void PageContentExtractionService::OnVisibilityChanged(
    std::optional<int64_t> tab_id,
    content::WebContents* web_contents,
    content::Visibility visibility) {
  if (!is_page_content_cache_enabled_) {
    return;
  }

  AnnotatedPageContentRequest* request =
      GetAnnotatedPageContentRequestFromWebContents(web_contents);
  if (!request) {
    return;
  }

  std::optional<ExtractedPageContentResult> extracted_result =
      request->GetCachedContentAndEligibility(/*log_metrics=*/false);
  if (extracted_result) {
    page_content_cache_handler_->OnVisibilityChanged(
        tab_id, ToWebStateWrapper(web_contents),
        ToPageContext(extracted_result->page_content->data, web_contents,
                      std::move(extracted_result->screenshot_data)),
        extracted_result->extraction_timestamp);
  }
}

void PageContentExtractionService::OnNewNavigation(
    std::optional<int64_t> tab_id,
    content::WebContents* web_contents,
    bool is_same_document) {
  if (is_page_content_cache_enabled_) {
    page_content_cache_handler_->OnNewNavigation(
        tab_id, ToWebStateWrapper(web_contents));
  }

  if (!is_same_document) {
    bool feature_enabled = base::FeatureList::IsEnabled(
        page_content_annotations::features::kAnnotatedPageContentExtraction);
    bool has_observers = !observers_.empty();

    EnablementSource source = EnablementSource::kNone;
    if (feature_enabled && has_observers) {
      source = EnablementSource::kBoth;
    } else if (feature_enabled) {
      source = EnablementSource::kFeatureFlag;
    } else if (has_observers) {
      source = EnablementSource::kObserverPresent;
    }

    base::UmaHistogramEnumeration(
        "OptimizationGuide.PageContentExtraction.EnablementSourcePerNavigation",
        source);
    base::UmaHistogramCounts100(
        "OptimizationGuide.PageContentExtraction.ObserverCountPerNavigation",
        std::distance(observers_.begin(), observers_.end()));
  }
}

void PageContentExtractionService::RunCleanUpTasksWithActiveTabs(
    const std::set<int64_t>& all_tab_ids) {
  if (is_page_content_cache_enabled_) {
    page_content_cache_handler_->page_content_cache()
        ->RunCleanUpTasksWithActiveTabs(all_tab_ids);
  }
}

bool PageContentExtractionService::IsOnDiskCacheEnabled() const {
  return is_page_content_cache_enabled_;
}

void PageContentExtractionService::GetPageContentFromOnDiskCache(
    int64_t tab_id,
    GetPageContentCallback callback) {
  if (is_page_content_cache_enabled_) {
    page_content_cache_handler_->page_content_cache()->GetPageContentForTab(
        tab_id, std::move(callback));
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

void PageContentExtractionService::GetOnDiskCachedTabIds(
    GetAllTabIdsCallback callback) {
  if (is_page_content_cache_enabled_) {
    page_content_cache_handler_->page_content_cache()->GetAllTabIds(
        std::move(callback));
  } else {
    std::move(callback).Run({});
  }
}

AnnotatedPageContentRequest*
PageContentExtractionService::GetAnnotatedPageContentRequestFromWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  return AnnotatedPageContentRequest::FromWebContents(web_contents);
}

AnnotatedPageContentRequest*
PageContentExtractionService::GetAnnotatedPageContentRequestFromPage(
    content::Page& page) {
  return GetAnnotatedPageContentRequestFromWebContents(
      content::WebContents::FromRenderFrameHost(&page.GetMainDocument()));
}

}  // namespace page_content_annotations
