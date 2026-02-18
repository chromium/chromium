// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTENT_EXTRACTION_SERVICE_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTENT_EXTRACTION_SERVICE_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

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

  // Returns the cached APC for `page` and whether it is eligible for
  // server upload. Will return nullopt if not available.
  // Virtual for testing.
  virtual std::optional<ExtractedPageContentResult>
  GetExtractedPageContentAndEligibilityForPage(content::Page& page);

  // Returns whether the cached APC for `page` is eligible for server upload.
  // Will return nullopt if not available.
  // Virtual for testing.
  virtual std::optional<bool> GetServerUploadEligibilityForPage(
      content::Page& page);

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

  base::ObserverList<Observer> observers_;

  const bool is_page_content_cache_enabled_;
  const std::unique_ptr<PageContentCacheHandler> page_content_cache_handler_;

 private:
  base::WeakPtrFactory<PageContentExtractionService> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTENT_EXTRACTION_SERVICE_H_
