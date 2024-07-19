// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_CONTENTS_WEB_APP_DATA_RETRIEVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_CONTENTS_WEB_APP_DATA_RETRIEVER_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_icon_downloader.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "components/webapps/common/web_page_metadata.mojom-forward.h"
#include "components/webapps/common/web_page_metadata_agent.mojom-forward.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

namespace content {
class WebContents;
}

namespace webapps {
struct InstallableData;
}

namespace web_app {

enum class IconsDownloadedResult;

struct WebAppInstallInfo;

// Class used by the WebApp system to retrieve the necessary information to
// install an app. Should only be called from the UI thread.
class WebAppDataRetriever : content::WebContentsObserver {
 public:
  // Returns nullptr for WebAppInstallInfo if error.
  using GetWebAppInstallInfoCallback =
      base::OnceCallback<void(std::unique_ptr<WebAppInstallInfo>)>;
  // |error_code| is the result of the installability check.
  // |webapps::InstallableStatusCode::NO_ERROR_DETECTED| equates to positive
  // installability. All other values correspond to negative installability and
  // first specific error encountered.
  // If |error_code| is |webapps::InstallableStatusCode::NO_ERROR_DETECTED| then
  // |valid_manifest_for_web_app| must be true.
  // If manifest is present then it is non-empty.
  // |manifest_url| is empty if manifest is empty.
  using CheckInstallabilityCallback =
      base::OnceCallback<void(blink::mojom::ManifestPtr opt_manifest,
                              bool valid_manifest_for_web_app,
                              webapps::InstallableStatusCode)>;

  using GetIconsCallback = WebAppIconDownloader::WebAppIconDownloaderCallback;

  static void PopulateWebAppInfoFromMetadata(
      WebAppInstallInfo* install_info,
      const webapps::mojom::WebPageMetadata& metadata);

  WebAppDataRetriever();
  WebAppDataRetriever(const WebAppDataRetriever&) = delete;
  WebAppDataRetriever& operator=(const WebAppDataRetriever&) = delete;
  ~WebAppDataRetriever() override;

  // Runs `callback` with a `WebAppInstallInfo` generated from the
  // `web_contents`. This tries to populate the following fields based on both
  // the `web_contents` and its `WebPageMetadata`: title, description,
  // start_url, icons, and mobile_capable.
  virtual void GetWebAppInstallInfo(content::WebContents* web_contents,
                                    GetWebAppInstallInfoCallback callback);

  // Performs installability checks and invokes `callback` with the contents of
  // the first manifest linked in the document.
  virtual void CheckInstallabilityAndRetrieveManifest(
      content::WebContents* web_contents,
      CheckInstallabilityCallback callback,
      std::optional<webapps::InstallableParams> params = std::nullopt);

  // Downloads icons from |icon_urls|. Runs |callback| with a map of
  // the retrieved icons.
  virtual void GetIcons(content::WebContents* web_contents,
                        const IconUrlSizeSet& extra_favicon_urls,
                        bool skip_page_favicons,
                        bool fail_all_if_any_fail,
                        GetIconsCallback callback);

  // WebContentsObserver:
  void WebContentsDestroyed() override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

 private:
  void OnGetWebPageMetadata(
      mojo::AssociatedRemote<webapps::mojom::WebPageMetadataAgent>
          metadata_agent,
      int last_committed_nav_entry_unique_id,
      webapps::mojom::WebPageMetadataPtr web_page_metadata);
  void OnDidPerformInstallableCheck(const webapps::InstallableData& data);
  void OnIconsDownloaded(IconsDownloadedResult result,
                         IconsMap icons_map,
                         DownloadedIconsHttpResults icons_http_results);

  void CallCallbackOnError(webapps::InstallableStatusCode error_code);
  bool ShouldStopRetrieval() const;

  std::unique_ptr<WebAppInstallInfo> fallback_install_info_;
  GetWebAppInstallInfoCallback get_web_app_info_callback_;

  CheckInstallabilityCallback check_installability_callback_;

  GetIconsCallback get_icons_callback_;

  std::unique_ptr<WebAppIconDownloader> icon_downloader_;

  base::WeakPtrFactory<WebAppDataRetriever> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_CONTENTS_WEB_APP_DATA_RETRIEVER_H_
