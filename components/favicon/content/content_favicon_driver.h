// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CONTENT_CONTENT_FAVICON_DRIVER_H_
#define COMPONENTS_FAVICON_CONTENT_CONTENT_FAVICON_DRIVER_H_

#include <vector>

#include "components/favicon/core/favicon_driver_impl.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "url/gurl.h"

namespace favicon {

class CoreFaviconService;

// ContentFaviconDriver is an implementation of FaviconDriver that listens to
// WebContents events to start download of favicons and to get informed when the
// favicon download has completed.
class ContentFaviconDriver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ContentFaviconDriver>,
      public FaviconDriverImpl {
 public:
  ContentFaviconDriver(const ContentFaviconDriver&) = delete;
  ContentFaviconDriver& operator=(const ContentFaviconDriver&) = delete;

  ~ContentFaviconDriver() override;

  // FaviconDriver implementation.
  gfx::Image GetFavicon() const override;
  bool FaviconIsValid() const override;
  GURL GetActiveURL() override;

  GURL GetManifestURL(content::RenderFrameHost* rfh);

 protected:
  ContentFaviconDriver(content::WebContents* web_contents,
                       CoreFaviconService* favicon_service);

 private:
  friend class content::WebContentsUserData<ContentFaviconDriver>;

  // TODO(crbug.com/40180290): these two classes are current used to ensure that
  // we disregard manifest URL updates that arrive prior to onload firing.
  struct DocumentManifestData
      : public content::DocumentUserData<DocumentManifestData> {
    explicit DocumentManifestData(content::RenderFrameHost* rfh);
    ~DocumentManifestData() override;
    DOCUMENT_USER_DATA_KEY_DECL();
    bool has_manifest_url = false;
  };

  struct NavigationManifestData
      : public content::NavigationHandleUserData<NavigationManifestData> {
    explicit NavigationManifestData(
        content::NavigationHandle& navigation_handle);
    ~NavigationManifestData() override;
    NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
    bool has_manifest_url = false;
  };

  // Callback when a manifest is downloaded.
  void OnDidDownloadManifest(ManifestDownloadCallback callback,
                             blink::mojom::ManifestRequestResult result,
                             const GURL& manifest_url,
                             blink::mojom::ManifestPtr manifest);

  // FaviconHandler::Delegate implementation.
  int DownloadImage(const GURL& url,
                    int max_image_size,
                    ImageDownloadCallback callback) override;
  void DownloadManifest(const GURL& url,
                        ManifestDownloadCallback callback) override;
  bool IsOffTheRecord() override;
  void OnFaviconUpdated(const GURL& page_url,
                        FaviconDriverObserver::NotificationIconType icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;
  void OnFaviconDeleted(const GURL& page_url,
                        FaviconDriverObserver::NotificationIconType
                            notification_icon_type) override;

  // content::WebContentsObserver implementation.
  void DidUpdateFaviconURL(
      content::RenderFrameHost* rfh,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override;
  void DidUpdateWebManifestURL(content::RenderFrameHost* rfh,
                               const GURL& manifest_url) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  GURL bypass_cache_page_url_;

  base::WeakPtrFactory<ContentFaviconDriver> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CONTENT_CONTENT_FAVICON_DRIVER_H_
