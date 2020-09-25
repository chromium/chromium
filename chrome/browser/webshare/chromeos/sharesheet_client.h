// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_CHROMEOS_SHARESHEET_CLIENT_H_
#define CHROME_BROWSER_WEBSHARE_CHROMEOS_SHARESHEET_CLIENT_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace webshare {

class PrepareDirectoryTask;

// Chrome-OS implementation of navigator.share() sharing to
// sharesheet::SharesheetService.
class SharesheetClient : public content::WebContentsObserver {
 public:
  using SharesheetCallback = base::RepeatingCallback<blink::mojom::ShareError(
      content::WebContents* web_contents,
      std::vector<GURL> file_urls,
      std::vector<std::string> content_types)>;

  explicit SharesheetClient(content::WebContents* web_contents);
  SharesheetClient(const SharesheetClient&) = delete;
  SharesheetClient& operator=(const SharesheetClient&) = delete;
  ~SharesheetClient() override;

  void Share(const std::string& title,
             const std::string& text,
             const GURL& share_url,
             std::vector<blink::mojom::SharedFilePtr> files,
             blink::mojom::ShareService::ShareCallback callback);

  static void SetSharesheetCallbackForTesting(SharesheetCallback callback);

 private:
  void OnPrepareDirectory(blink::mojom::ShareError);

  void OnStoreFiles(blink::mojom::ShareError);

  static blink::mojom::ShareError ShowSharesheet(
      content::WebContents* web_contents,
      std::vector<GURL> file_urls,
      std::vector<std::string> content_types);

  static SharesheetCallback& GetSharesheetCallback();

  // WebContentsObserver:
  void WebContentsDestroyed() override;

  struct CurrentShare {
    CurrentShare();
    CurrentShare(CurrentShare&&);
    CurrentShare& operator=(CurrentShare&&);
    CurrentShare(const CurrentShare&) = delete;
    CurrentShare& operator=(const CurrentShare&) = delete;
    ~CurrentShare();

    std::vector<blink::mojom::SharedFilePtr> files;
    base::FilePath directory;
    std::vector<GURL> file_urls;
    std::vector<std::string> content_types;
    blink::mojom::ShareService::ShareCallback callback;

    std::unique_ptr<PrepareDirectoryTask> prepare_directory_task;
  };

  base::Optional<CurrentShare> current_share_;

  base::WeakPtrFactory<SharesheetClient> weak_ptr_factory_{this};
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_CHROMEOS_SHARESHEET_CLIENT_H_
