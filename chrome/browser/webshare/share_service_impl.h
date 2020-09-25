// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_SHARE_SERVICE_IMPL_H_
#define CHROME_BROWSER_WEBSHARE_SHARE_SERVICE_IMPL_H_

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/webshare/chromeos/sharesheet_client.h"
#endif

class GURL;

namespace content {
class RenderFrameHost;
}

constexpr size_t kMaxSharedFileCount = 10;
constexpr uint64_t kMaxSharedFileBytes = 50 * 1024 * 1024;

class ShareServiceImpl : public blink::mojom::ShareService,
                         public content::WebContentsObserver {
 public:
  explicit ShareServiceImpl(content::RenderFrameHost& render_frame_host);
  ShareServiceImpl(const ShareServiceImpl&) = delete;
  ShareServiceImpl& operator=(const ShareServiceImpl&) = delete;
  ~ShareServiceImpl() override;

  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::ShareService> receiver);

  static bool IsDangerousFilename(base::StringPiece);
  static bool IsDangerousMimeType(base::StringPiece);

  // blink::mojom::ShareService:
  void Share(const std::string& title,
             const std::string& text,
             const GURL& share_url,
             std::vector<blink::mojom::SharedFilePtr> files,
             ShareCallback callback) override;

  // content::WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

 private:
#if defined(OS_CHROMEOS)
  webshare::SharesheetClient sharesheet_client_;
#endif
  content::RenderFrameHost* render_frame_host_;
};

#endif  // CHROME_BROWSER_WEBSHARE_SHARE_SERVICE_IMPL_H_
