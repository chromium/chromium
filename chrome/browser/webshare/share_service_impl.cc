// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/share_service_impl.h"

#include "base/feature_list.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

ShareServiceImpl::ShareServiceImpl(content::RenderFrameHost& render_frame_host)
    : content::WebContentsObserver(
          content::WebContents::FromRenderFrameHost(&render_frame_host)),
      render_frame_host_(&render_frame_host) {
  DCHECK(base::FeatureList::IsEnabled(features::kWebShare));
}

ShareServiceImpl::~ShareServiceImpl() = default;

// static
void ShareServiceImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::ShareService> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ShareServiceImpl>(*render_frame_host),
      std::move(receiver));
}

void ShareServiceImpl::Share(const std::string& title,
                             const std::string& text,
                             const GURL& share_url,
                             std::vector<blink::mojom::SharedFilePtr> files,
                             ShareCallback callback) {
  if (files.size() > kMaxSharedFileCount) {
    std::move(callback).Run(blink::mojom::ShareError::PERMISSION_DENIED);
    return;
  }

  uint64_t total_bytes = 0;
  for (auto& file : files) {
    if (!file || !file->blob) {
      mojo::ReportBadMessage("Invalid file to share()");
      return;
    }
    total_bytes += file->blob->size;
  }

  if (total_bytes > kMaxSharedFileBytes) {
    std::move(callback).Run(blink::mojom::ShareError::PERMISSION_DENIED);
    return;
  }

  // TODO(crbug.com/1035527): Add implementation for OS_WIN
  // TODO(crbug.com/1110119): Add implementation for OS_CHROMEOS
  NOTIMPLEMENTED();
  std::move(callback).Run(blink::mojom::ShareError::CANCELED);
}

void ShareServiceImpl::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host == render_frame_host_)
    render_frame_host_ = nullptr;
}
