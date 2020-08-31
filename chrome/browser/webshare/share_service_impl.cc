// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/share_service_impl.h"

#include "mojo/public/cpp/bindings/self_owned_receiver.h"

ShareServiceImpl::ShareServiceImpl() = default;
ShareServiceImpl::~ShareServiceImpl() = default;

// static
void ShareServiceImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::ShareService> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<ShareServiceImpl>(),
                              std::move(receiver));
}

void ShareServiceImpl::Share(const std::string& title,
                             const std::string& text,
                             const GURL& share_url,
                             std::vector<blink::mojom::SharedFilePtr> files,
                             ShareCallback callback) {
  // TODO(crbug.com/1035527): Add implementation for WIN_OS
  NOTIMPLEMENTED();
  std::move(callback).Run(blink::mojom::ShareError::CANCELED);
}
