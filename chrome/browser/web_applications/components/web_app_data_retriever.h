// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_DATA_RETRIEVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_DATA_RETRIEVER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/web_app_icon_downloader.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "components/webapps/common/web_page_metadata.mojom-forward.h"
#include "components/webapps/common/web_page_metadata_agent.mojom-forward.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

class GURL;
struct WebApplicationInfo;

namespace blink {
struct Manifest;
}

namespace content {
class WebContents;
}

namespace webapps {
struct InstallableData;
}

namespace web_app {

// Class used by WebAppInstallTask to retrieve the necessary information to
// install an app. Should only be called from the UI thread.
class WebAppDataRetriever : content::WebContentsObserver {
 public:
  // Returns nullptr for WebApplicationInfo if error.
  using GetWebApplicationInfoCallback =
      base::OnceCallback<void(std::unique_ptr<WebApplicationInfo>)>;
  // |is_installable| represents installability check result.
  // If |is_installable| then |valid_manifest_for_web_app| is true.
  // If manifest is present then it is non-empty.
  // |manifest_url| is empty if manifest is empty.
  using CheckInstallabilityCallback =
      base::OnceCallback<void(base::Optional<blink::Manifest> manifest,
                              const GURL& manifest_url,
                              bool valid_manifest_for_web_app,
                              bool is_installable)>;
  // Returns empty map if error.
  using GetIconsCallback = base::OnceCallback<void(IconsMap)>;

  WebAppDataRetriever();
  WebAppDataRetriever(const WebAppDataRetriever&) = delete;
  WebAppDataRetriever& operator=(const WebAppDataRetriever&) = delete;
  ~WebAppDataRetriever() override;

  // Runs |callback| with the result of retrieving the WebApplicationInfo from
  // |web_contents|.
  virtual void GetWebApplicationInfo(content::WebContents* web_contents,
                                     GetWebApplicationInfoCallback callback);

  // Performs installability check and invokes |callback| with manifest.
  virtual void CheckInstallabilityAndRetrieveManifest(
      content::WebContents* web_contents,
      bool bypass_service_worker_check,
      CheckInstallabilityCallback callback);

  // Downloads icons from |icon_urls|. Runs |callback| with a map of
  // the retrieved icons.
  virtual void GetIcons(content::WebContents* web_contents,
                        const std::vector<GURL>& icon_urls,
                        bool skip_page_favicons,
                        WebAppIconDownloader::Histogram histogram,
                        GetIconsCallback callback);

  // WebContentsObserver:
  void WebContentsDestroyed() override;
  void RenderProcessGone(base::TerminationStatus status) override;

 private:
  void OnGetWebPageMetadata(
      mojo::AssociatedRemote<webapps::mojom::WebPageMetadataAgent>
          metadata_agent,
      int last_committed_nav_entry_unique_id,
      webapps::mojom::WebPageMetadataPtr web_page_metadata);
  void OnDidPerformInstallableCheck(const webapps::InstallableData& data);
  void OnIconsDownloaded(bool success, IconsMap icons_map);

  void CallCallbackOnError();
  bool ShouldStopRetrieval() const;

  std::unique_ptr<WebApplicationInfo> default_web_application_info_;
  GetWebApplicationInfoCallback get_web_app_info_callback_;

  CheckInstallabilityCallback check_installability_callback_;
  GetIconsCallback get_icons_callback_;

  std::unique_ptr<WebAppIconDownloader> icon_downloader_;

  base::WeakPtrFactory<WebAppDataRetriever> weak_ptr_factory_{this};

};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_DATA_RETRIEVER_H_
