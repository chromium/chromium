// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_CHROMEOS_SHARESHEET_CLIENT_H_
#define CHROME_BROWSER_WEBSHARE_CHROMEOS_SHARESHEET_CLIENT_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chromeos/components/sharesheet/constants.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace webshare {

class PrepareDirectoryTask;
class PrepareSubDirectoryTask;

// Chrome-OS implementation of navigator.share() sharing to
// sharesheet::SharesheetService.
class SharesheetClient : public content::WebContentsObserver {
 public:
  using DeliveredCallback = sharesheet::DeliveredCallback;
  using SharesheetCallback = base::RepeatingCallback<void(
      content::WebContents* web_contents,
      const std::vector<base::FilePath>& file_paths,
      const std::vector<std::string>& content_types,
      const std::vector<uint64_t>& file_sizes,
      const std::string& text,
      const std::string& title,
      DeliveredCallback delivered_callback)>;

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

  void OnPrepareSubdirectory(blink::mojom::ShareError);

  void OnStoreFiles(blink::mojom::ShareError);

  void OnShowSharesheet(sharesheet::SharesheetResult result);

  static void ShowSharesheet(content::WebContents* web_contents,
                             const std::vector<base::FilePath>& file_paths,
                             const std::vector<std::string>& content_types,
                             const std::vector<uint64_t>& file_sizes,
                             const std::string& text,
                             const std::string& title,
                             DeliveredCallback delivered_callback);

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
    std::vector<base::FilePath> file_paths;
    std::vector<std::string> content_types;
    std::vector<uint64_t> file_sizes;
    std::string text;
    std::string title;
    blink::mojom::ShareService::ShareCallback callback;

    std::unique_ptr<PrepareDirectoryTask> prepare_directory_task;
    std::unique_ptr<PrepareSubDirectoryTask> prepare_subdirectory_task;
  };

  std::optional<CurrentShare> current_share_;

  base::WeakPtrFactory<SharesheetClient> weak_ptr_factory_{this};
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_CHROMEOS_SHARESHEET_CLIENT_H_
