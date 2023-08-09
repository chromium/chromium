// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_FAKE_INSTALLABLE_MANAGER_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_FAKE_INSTALLABLE_MANAGER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace webapps {
struct InstallableData;

class FakeInstallableManager : public InstallableManager {
 public:
  explicit FakeInstallableManager(content::WebContents* web_contents);
  ~FakeInstallableManager() override;

  // InstallableManager:
  void GetData(const InstallableParams& params,
               InstallableCallback callback) override;

  void RunCallback(InstallableCallback callback);

  // Create the manager and attach it to |web_contents|.
  static FakeInstallableManager* CreateForWebContents(
      content::WebContents* web_contents);

  // Create the manager and attach it to |web_contents|. It responds to
  // |GetData| with a blink |manifest| provided.
  static FakeInstallableManager* CreateForWebContentsWithManifest(
      content::WebContents* web_contents,
      InstallableStatusCode installable_code,
      const GURL& manifest_url,
      blink::mojom::ManifestPtr manifest);

 private:
  GURL manifest_url_;
  blink::mojom::ManifestPtr manifest_;
  mojom::WebPageMetadataPtr web_page_metadata_;
  std::unique_ptr<InstallableData> data_;

  base::WeakPtrFactory<FakeInstallableManager> weak_factory_{this};
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_FAKE_INSTALLABLE_MANAGER_H_
