// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/share_service_impl.h"

#include <algorithm>
#include <memory>

#include "base/feature_list.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if defined(OS_MAC)
#include "chrome/browser/webshare/mac/sharing_service_operation.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/webshare/win/share_operation.h"
#endif

// IsDangerousFilename() and IsDangerousMimeType() should be kept in sync with
// //third_party/blink/renderer/modules/webshare/FILE_TYPES.md
// //components/browser_ui/webshare/android/java/src/org/chromium/components/browser_ui/webshare/ShareServiceImpl.java

ShareServiceImpl::ShareServiceImpl(content::RenderFrameHost& render_frame_host)
    : content::WebContentsObserver(
          content::WebContents::FromRenderFrameHost(&render_frame_host)),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      sharesheet_client_(web_contents()),
#endif
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

// static
bool ShareServiceImpl::IsDangerousFilename(base::StringPiece name) {
  constexpr std::array<const char*, 39> kPermitted = {
      ".bmp",    // image/bmp / image/x-ms-bmp
      ".css",    // text/css
      ".csv",    // text/csv / text/comma-separated-values
      ".ehtml",  // text/html
      ".flac",   // audio/flac
      ".gif",    // image/gif
      ".htm",    // text/html
      ".html",   // text/html
      ".ico",    // image/x-icon
      ".jfif",   // image/jpeg
      ".jpeg",   // image/jpeg
      ".jpg",    // image/jpeg
      ".m4a",    // audio/x-m4a
      ".m4v",    // video/mp4
      ".mp3",    // audio/mpeg audio/mp3
      ".mp4",    // video/mp4
      ".mpeg",   // video/mpeg
      ".mpg",    // video/mpeg
      ".oga",    // audio/ogg
      ".ogg",    // audio/ogg
      ".ogm",    // video/ogg
      ".ogv",    // video/ogg
      ".opus",   // audio/ogg
      ".pjp",    // image/jpeg
      ".pjpeg",  // image/jpeg
      ".png",    // image/png
      ".shtm",   // text/html
      ".shtml",  // text/html
      ".svg",    // image/svg+xml
      ".svgz",   // image/svg+xml
      ".text",   // text/plain
      ".tif",    // image/tiff
      ".tiff",   // image/tiff
      ".txt",    // text/plain
      ".wav",    // audio/wav
      ".weba",   // audio/webm
      ".webm",   // video/webm
      ".webp",   // image/webp
      ".xbm",    // image/x-xbitmap
  };

  for (const char* permitted : kPermitted) {
    if (base::EndsWith(name, permitted, base::CompareCase::INSENSITIVE_ASCII))
      return false;
  }
  return true;
}

// static
bool ShareServiceImpl::IsDangerousMimeType(base::StringPiece content_type) {
  constexpr std::array<const char*, 26> kPermitted = {
      "audio/flac",
      "audio/mp3",
      "audio/mpeg",
      "audio/ogg",
      "audio/wav",
      "audio/webm",
      "audio/x-m4a",
      "image/bmp",
      "image/gif",
      "image/jpeg",
      "image/png",
      "image/svg+xml",
      "image/tiff",
      "image/webp",
      "image/x-icon",
      "image/x-ms-bmp",
      "image/x-xbitmap",
      "text/comma-separated-values",
      "text/css",
      "text/csv",
      "text/html",
      "text/plain",
      "video/mp4",
      "video/mpeg",
      "video/ogg",
      "video/webm",
  };

  for (const char* permitted : kPermitted) {
    if (content_type == permitted)
      return false;
  }
  return true;
}

void ShareServiceImpl::Share(const std::string& title,
                             const std::string& text,
                             const GURL& share_url,
                             std::vector<blink::mojom::SharedFilePtr> files,
                             ShareCallback callback) {
  content::WebContents* const web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  if (!web_contents) {
    VLOG(1) << "Cannot share after navigating away";
    std::move(callback).Run(blink::mojom::ShareError::PERMISSION_DENIED);
    return;
  }

  if (files.size() > kMaxSharedFileCount) {
    VLOG(1) << "Share too large: " << files.size() << " files";
    std::move(callback).Run(blink::mojom::ShareError::PERMISSION_DENIED);
    return;
  }

  for (auto& file : files) {
    if (!file || !file->blob || !file->blob->blob) {
      mojo::ReportBadMessage("Invalid file to share()");
      return;
    }

    if (IsDangerousFilename(file->name) ||
        IsDangerousMimeType(file->blob->content_type)) {
      VLOG(1) << "File type is not supported: " << file->name
              << " has mime type " << file->blob->content_type;
      std::move(callback).Run(blink::mojom::ShareError::PERMISSION_DENIED);
      return;
    }

    // In the case where the original blob handle was to a native file (of
    // unknown size), the serialized data does not contain an accurate file
    // size. To handle this, the comparison against kMaxSharedFileBytes should
    // be done by the platform-specific implementations as part of processing
    // the blobs.
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  sharesheet_client_.Share(title, text, share_url, std::move(files),
                           std::move(callback));
#elif defined(OS_MAC)
  auto sharing_service_operation =
      std::make_unique<webshare::SharingServiceOperation>(
          title, text, share_url, std::move(files), web_contents);

  // grab a safe reference to |sharing_service_operation| before calling move on
  // it.
  webshare::SharingServiceOperation* sharing_service_operation_ptr =
      sharing_service_operation.get();

  // Wrap the |callback| in a binding that owns the |sharing_service_operation|
  // so its lifetime can be preserved till its done.
  sharing_service_operation_ptr->Share(base::BindOnce(
      [](std::unique_ptr<webshare::SharingServiceOperation>
             sharing_service_operation,
         ShareCallback callback,
         blink::mojom::ShareError result) { std::move(callback).Run(result); },
      std::move(sharing_service_operation), std::move(callback)));
#elif defined(OS_WIN)
  auto share_operation = std::make_unique<webshare::ShareOperation>(
      title, text, share_url, std::move(files), web_contents);
  auto* const share_operation_ptr = share_operation.get();
  share_operation_ptr->Run(base::BindOnce(
      [](std::unique_ptr<webshare::ShareOperation> share_operation,
         ShareCallback callback,
         blink::mojom::ShareError result) { std::move(callback).Run(result); },
      std::move(share_operation), std::move(callback)));
#else
  NOTREACHED();
  std::move(callback).Run(blink::mojom::ShareError::INTERNAL_ERROR);
#endif
}

void ShareServiceImpl::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host == render_frame_host_)
    render_frame_host_ = nullptr;
}
