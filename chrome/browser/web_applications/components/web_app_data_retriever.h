// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_DATA_RETRIEVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_DATA_RETRIEVER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/web_app_icon_downloader.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

class GURL;
struct InstallableData;
struct WebApplicationInfo;

namespace blink {
struct Manifest;
}

namespace content {
class WebContents;
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
  // If |valid_manifest_for_web_app| then manifest is present and non-empty.
  using CheckInstallabilityCallback =
      base::OnceCallback<void(base::Optional<blink::Manifest> manifest,
                              bool valid_manifest_for_web_app,
                              bool is_installable)>;
  // Returns empty map if error.
  using GetIconsCallback = base::OnceCallback<void(IconsMap)>;

  WebAppDataRetriever();
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
  void OnGetWebApplicationInfo(
      mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
          chrome_render_frame,
      int last_committed_nav_entry_unique_id,
      const WebApplicationInfo& web_app_info);
  void OnDidPerformInstallableCheck(const InstallableData& data);
  void OnIconsDownloaded(bool success, IconsMap icons_map);

  void CallCallbackOnError();
  bool ShouldStopRetrieval() const;

  GetWebApplicationInfoCallback get_web_app_info_callback_;
  CheckInstallabilityCallback check_installability_callback_;
  GetIconsCallback get_icons_callback_;

  std::unique_ptr<WebAppIconDownloader> icon_downloader_;

  base::WeakPtrFactory<WebAppDataRetriever> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppDataRetriever);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_DATA_RETRIEVER_H_
