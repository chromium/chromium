// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_CONTENTS_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_CONTENTS_MANAGER_H_

#include <map>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_app_icon_downloader.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

class GURL;
class SkBitmap;

namespace content {
class WebContents;
}

namespace web_app {

// This class facilities creating WebAppUrlLoaders and WebAppDataRetrievers that
// reflect a fake network state. This class can be re-used when creating a
// general dependency wrapper for the web contents system, see
// http://b/262606416.
class FakeWebContentsManager {
 public:
  // State used to represent a page at a url, which is retrieved through
  // `LoadUrl`, `GetWebAppInstallInfo`, and
  // `CheckInstallabilityAndRetrieveManifest`.
  struct FakePageState {
    FakePageState();
    ~FakePageState();
    FakePageState(FakePageState&&);

    // `WebAppUrlLoader::LoadUrl`:
    // If this is populated, then a redirection is always assumed. If the
    // redirection is allowed by the `LoadUrlComparison`, then `url_load_result`
    // will be given as the result. Otherwise,`kRedirectedUrlLoaded` is
    // returned.
    absl::optional<GURL> redirection_url = absl::nullopt;
    WebAppUrlLoaderResult url_load_result =
        WebAppUrlLoaderResult::kFailedErrorPageLoaded;

    // `WebAppDataRetriever::GetWebAppInstallInfo`:
    std::unique_ptr<WebAppInstallInfo> page_install_info;

    // `WebAppDataRetriever::CheckInstallabilityAndRetrieveManifest`:
    bool has_service_worker = false;
    // An empty url is considered an absent url.
    GURL manifest_url;
    bool valid_manifest_for_web_app = false;
    blink::mojom::ManifestPtr opt_manifest;
    webapps::InstallableStatusCode error_code;
    GURL favicon_url;
    std::vector<SkBitmap> favicon;

    base::OnceClosure on_manifest_fetch;
  };

  // State used to represent an icon at a url, which is retrieved through
  // `GetIcons`.
  struct FakeIconState {
    FakeIconState();
    ~FakeIconState();

    int http_status_code = 200;
    std::vector<SkBitmap> bitmaps;
    // This can be used to test the early-exit normally caused by the
    // WebContents closing or navigating away.
    bool trigger_primary_page_changed_if_fetched = false;
  };

  FakeWebContentsManager();
  ~FakeWebContentsManager();

  void SetUrlLoaded(content::WebContents* web_contents, const GURL& url);

  std::unique_ptr<WebAppUrlLoader> CreateUrlLoader();
  std::unique_ptr<WebAppDataRetriever> CreateDataRetriever();
  std::unique_ptr<WebAppIconDownloader> CreateIconDownloader();

  // Set the behavior for calls to `GetIcons` from wrappers returned by this
  // fake class.
  void SetIconState(const GURL& icon_url, const FakeIconState& icon_state);
  FakeIconState& GetOrCreateIconState(const GURL& icon_url);
  void DeleteIconState(const GURL& icon_url);

  // Set the behavior for calls to `LoadUrl`, `GetWebAppInstallInfo`, and
  // `CheckInstallabilityAndRetrieveManifest`  from wrappers returned by this
  // fake class.
  void SetPageState(const GURL& gurl, FakePageState page_state);
  FakePageState& GetOrCreatePageState(const GURL& gurl);
  void DeletePageState(const GURL& gurl);

  base::WeakPtr<FakeWebContentsManager> GetWeakPtr();

 private:
  class FakeUrlLoader;
  class FakeWebAppDataRetriever;

  std::map<GURL, FakeIconState> icon_state_;
  std::map<GURL, FakePageState> page_state_;

  // This is a hack that can be removed once we centralize WebContents usage
  // through one wrapper class. This keeps track of the last loaded url for the
  // given web contents pointer, allowing the data retriver to know what the url
  // loader did.
  std::map<content::WebContents*, GURL> loaded_urls_;

  base::WeakPtrFactory<FakeWebContentsManager> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_CONTENTS_MANAGER_H_
