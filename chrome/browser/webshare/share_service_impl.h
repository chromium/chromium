// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_SHARE_SERVICE_IMPL_H_
#define CHROME_BROWSER_WEBSHARE_SHARE_SERVICE_IMPL_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/webshare/safe_browsing_request.h"
#include "content/public/browser/document_service.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/webshare/chromeos/sharesheet_client.h"
#endif

class GURL;

namespace content {
class RenderFrameHost;
}

enum class WebShareMethod { kShare = 0, kMaxValue = kShare };

// UMA metric name for Web Share API count.
constexpr const char* kWebShareApiCountMetric = "WebShare.ApiCount";

constexpr size_t kMaxSharedFileCount = 10;
constexpr uint64_t kMaxSharedFileBytes = 50 * 1024 * 1024;

class ShareServiceImpl
    : public content::DocumentService<blink::mojom::ShareService> {
 public:
  ShareServiceImpl(const ShareServiceImpl&) = delete;
  ShareServiceImpl& operator=(const ShareServiceImpl&) = delete;

  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::ShareService> receiver);

  static bool IsDangerousFilename(const base::FilePath& path);
  static bool IsDangerousMimeType(std::string_view content_type);

  // blink::mojom::ShareService:
  void Share(const std::string& title,
             const std::string& text,
             const GURL& share_url,
             std::vector<blink::mojom::SharedFilePtr> files,
             ShareCallback callback) override;

 private:
  void OnSafeBrowsingResultReceived(
      const std::string& title,
      const std::string& text,
      const GURL& share_url,
      std::vector<blink::mojom::SharedFilePtr> files,
      ShareCallback callback,
      bool is_safe);

  ShareServiceImpl(content::RenderFrameHost& render_frame_host,
                   mojo::PendingReceiver<blink::mojom::ShareService> receiver);
  ~ShareServiceImpl() override;

  std::optional<SafeBrowsingRequest> safe_browsing_request_;

#if BUILDFLAG(IS_CHROMEOS)
  webshare::SharesheetClient sharesheet_client_;
#endif

  base::WeakPtrFactory<ShareServiceImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_WEBSHARE_SHARE_SERVICE_IMPL_H_
