// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTENT_EXTRACTION_SERVICE_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTENT_EXTRACTION_SERVICE_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace base {
class FilePath;
}  // namespace base

namespace content {
class Page;
class WebContents;
enum class Visibility;
}  // namespace content

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

namespace os_crypt_async {
class OSCryptAsync;
}  // namespace os_crypt_async

namespace page_content_annotations {

using RefCountedAnnotatedPageContent =
    base::RefCountedData<optimization_guide::proto::AnnotatedPageContent>;

class AnnotatedPageContentRequest;
struct ExtractedPageContentResult;
class PageContentCache;
class PageContentCacheHandler;

class PageContentExtractionService : public KeyedService,
                                     public base::SupportsUserData {
 public:
  using GetExtractedPageContentAndEligibilityCallback =
      base::OnceCallback<void(std::optional<ExtractedPageContentResult>)>;
  using GetServerUploadEligibilityCallback =
      base::OnceCallback<void(std::optional<bool>)>;

  class Observer : public base::CheckedObserver {
   public:
    // Invoked when `page_content` is extracted for `page`. The extraction is
    // triggered for every page once the page has sufficiently loaded.
    virtual void OnPageContentExtracted(
        content::Page& page,
        scoped_refptr<const RefCountedAnnotatedPageContent> page_content) {}
  };

#if BUILDFLAG(IS_ANDROID)
  // Returns a Java object for the given service.
  static base::android::ScopedJavaLocalRef<jobject> GetJavaObject(
      PageContentExtractionService* service);
#endif  // BUILDFLAG(IS_ANDROID)

  PageContentExtractionService(os_crypt_async::OSCryptAsync* os_crypt_async,
                               const base::FilePath& profile_path,
                               feature_engagement::Tracker* tracker);
  ~PageContentExtractionService() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns whether page content extraction should be enabled. It should be
  // enabled based on features, or when some observer has registered for page
  // content.
  bool ShouldEnablePageContentExtraction() const;

  // TODO(b/490161242): Improve the behavior in these functions: allow for
  // constructing an AnnotatedPageContentRequest if one doesn't already exist,
  // and, if not constructible, return the reason why via a base::expected.

  // Returns the cached APC for `page` and whether it is eligible for
  // server upload. Will return nullopt if not available or not supported. E.g.
  // for PDFs, when initial extraction is not complete, the triggering mode is
  // 'on hidden' and the page is still visible, or the request object lacks
  // observers.
  // Virtual for testing.
  virtual std::optional<ExtractedPageContentResult>
  GetExtractedPageContentAndEligibilityForPage(content::Page& page);

  // Returns whether the cached APC for `page` is eligible for server upload.
  // Will return nullopt if not available. See
  // `GetExtractedPageContentAndEligibilityForPage` for possible causes.
  // Virtual for testing.
  virtual std::optional<bool> GetServerUploadEligibilityForPage(
      content::Page& page);

  // Asynchronous versions of the getter methods above.
  // These methods will resolve immediately if the extraction is already
  // complete, or wait for the initial extraction to finish if there is one
  // pending, or is not scheduled to occur. If the extraction request is
  // cleared or reset (e.g. from a navigation or destruction), the callbacks
  // will resolve with std::nullopt.
  // Virtual for testing.
  virtual void GetExtractedPageContentAndEligibilityForPageAsync(
      content::Page& page,
      GetExtractedPageContentAndEligibilityCallback callback);
  virtual void GetServerUploadEligibilityForPageAsync(
      content::Page& page,
      GetServerUploadEligibilityCallback callback);

  // Extracts a new APC for `page` and computes its eligibility for server
  // upload, and caches the new result. It will wait for the initial
  // extraction to complete if there is one pending. For PDFs, it will return
  // the cached copy instead. If the extraction request is cleared or reset
  // (e.g. from a navigation or destruction), the callbacks will resolve with
  // std::nullopt. Extraction is not supported for PDFs and will also result in
  // nullopt.
  // Virtual for testing.
  virtual void RefreshExtractedPageContentAndEligibilityForPage(
      content::Page& page,
      GetExtractedPageContentAndEligibilityCallback callback);

  // Called when a tab is closed.
  void OnTabClosed(int64_t tab_id);

  // Called when a closed tab is undone.
  void OnTabCloseUndone(int64_t tab_id);

  // Called when the visibility of a WebContents changes.
  void OnVisibilityChanged(std::optional<int64_t> tab_id,
                           content::WebContents* web_contents,
                           content::Visibility visibility);

  // Called when a new navigation happens in a WebContents.
  void OnNewNavigation(std::optional<int64_t> tab_id,
                       content::WebContents* web_contents);

  // Called when all the tab models are initialized to perform cleanup of stale
  // entries in the page content cache.
  void RunCleanUpTasksWithActiveTabs(const std::set<int64_t>& all_tab_ids);

  // Disk cache for getting page contents for tabs without webcontents.
  PageContentCache* GetPageContentCache();

 protected:
  friend class AnnotatedPageContentRequest;

  // Invoked when `page_content` is extracted for `page`, to notify the
  // observers. `tab_id` for the tab where page is loaded, if available.
  virtual void OnPageContentExtracted(
      content::Page& page,
      scoped_refptr<const RefCountedAnnotatedPageContent>
          annotated_page_content,
      const std::vector<uint8_t>& screenshot_data,
      std::optional<int> tab_id);

  AnnotatedPageContentRequest* GetAnnotatedPageContentRequestFromWebContents(
      content::WebContents* web_contents);
  AnnotatedPageContentRequest* GetAnnotatedPageContentRequestFromPage(
      content::Page& page);

  base::ObserverList<Observer> observers_;

  const bool is_page_content_cache_enabled_;
  const std::unique_ptr<PageContentCacheHandler> page_content_cache_handler_;

 private:
  base::WeakPtrFactory<PageContentExtractionService> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTENT_EXTRACTION_SERVICE_H_
