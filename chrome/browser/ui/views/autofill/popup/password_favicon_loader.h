// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_PASSWORD_FAVICON_LOADER_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_PASSWORD_FAVICON_LOADER_H_

#include "base/containers/lru_cache.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/ui/suggestion.h"

namespace base {
class CancelableTaskTracker;
}

namespace favicon {
class LargeIconService;
}

namespace favicon_base {
struct LargeIconResult;
}

namespace gfx {
class Image;
}

namespace image_fetcher {
class ImageFetcher;
struct RequestMetadata;
}  // namespace image_fetcher

class GURL;

namespace autofill {

// This class handles the process of loading favicons for websites associated
// with user credentials.
class PasswordFaviconLoader {
 public:
  using OnLoadSuccess = base::OnceCallback<void(const gfx::Image& image)>;
  using OnLoadFail = base::OnceCallback<void()>;

  virtual void Load(const Suggestion::FaviconDetails& favicon_details,
                    base::CancelableTaskTracker* task_tracker,
                    OnLoadSuccess on_success,
                    OnLoadFail on_fail) = 0;
};

class PasswordFaviconLoaderImpl : public PasswordFaviconLoader {
 public:
  PasswordFaviconLoaderImpl(favicon::LargeIconService* favicon_service,
                            image_fetcher::ImageFetcher* image_fetcher);
  virtual ~PasswordFaviconLoaderImpl();

  void Load(const Suggestion::FaviconDetails& favicon_details,
            base::CancelableTaskTracker* task_tracker,
            OnLoadSuccess on_success,
            OnLoadFail on_fail) override;

 private:
  void OnFaviconResponse(const GURL& domain_url,
                         OnLoadSuccess on_success,
                         OnLoadFail on_fail,
                         const favicon_base::LargeIconResult& result);
  void OnFaviconResponseFromImageFetcher(
      const GURL& domain_url,
      OnLoadSuccess on_success,
      OnLoadFail on_fail,
      const gfx::Image& image,
      const image_fetcher::RequestMetadata& request_metadata);

  const raw_ref<favicon::LargeIconService> favicon_service_;
  const raw_ref<image_fetcher::ImageFetcher> image_fetcher_;

  // The in-memory cache for icons downloaded through
  // `favicon::LargeIconService`. It allows to synchronously respond to
  // a favicon request and avoid image blinking on filter change (for those
  // icons that were downloaded but not filtered out). This happens because
  // the service API is asynchronous and we recreate the row view for every
  // filter change.
  base::LRUCache<GURL, gfx::Image> cache_;

  base::WeakPtrFactory<PasswordFaviconLoaderImpl> weak_ptr_factory_{this};
};
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_PASSWORD_FAVICON_LOADER_H_
