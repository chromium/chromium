// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_SHARE_SERVICE_IMPL_H_
#define CHROME_BROWSER_WEBSHARE_SHARE_SERVICE_IMPL_H_

#include <string>
#include <vector>

#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"

class GURL;

namespace content {
class RenderFrameHost;
}

class ShareServiceImpl : public blink::mojom::ShareService {
 public:
  ShareServiceImpl();
  ShareServiceImpl(const ShareServiceImpl&) = delete;
  ShareServiceImpl& operator=(const ShareServiceImpl&) = delete;
  ~ShareServiceImpl() override;

  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::ShareService> receiver);

  // blink::mojom::ShareService:
  void Share(const std::string& title,
             const std::string& text,
             const GURL& share_url,
             std::vector<blink::mojom::SharedFilePtr> files,
             ShareCallback callback) override;
};

#endif  // CHROME_BROWSER_WEBSHARE_SHARE_SERVICE_IMPL_H_
