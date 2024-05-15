// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/share_service_impl.h"

#include <algorithm>
#include <memory>
#include <string_view>

#include "base/feature_list.h"
#include "base/files/safe_base_name.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_features.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/webshare/mac/sharing_service_operation.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/webshare/win/share_operation.h"
#endif

// IsDangerousFilename() and IsDangerousMimeType() should be kept in sync with
// //third_party/blink/renderer/modules/webshare/FILE_TYPES.md
// //components/browser_ui/webshare/android/java/src/org/chromium/components/browser_ui/webshare/ShareServiceImpl.java

ShareServiceImpl::ShareServiceImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::ShareService> receiver)
    : content::DocumentService<blink::mojom::ShareService>(render_frame_host,
                                                           std::move(receiver))
#if BUILDFLAG(IS_CHROMEOS)
      ,
      sharesheet_client_(
          content::WebContents::FromRenderFrameHost(&render_frame_host))
#endif
{
  DCHECK(base::FeatureList::IsEnabled(features::kWebShare));
}

ShareServiceImpl::~ShareServiceImpl() = default;

// static
void ShareServiceImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::ShareService> receiver) {
  CHECK(render_frame_host);
  if (render_frame_host->IsNestedWithinFencedFrame()) {
    // The renderer should have checked and disallowed the request for fenced
    // frames in NavigatorShare and thrown a DOMException. Ignore the request
    // and mark it as bad if it didn't happen for some reason.
    bad_message::ReceivedBadMessage(render_frame_host->GetProcess(),
                                    bad_message::SSI_CREATE_FENCED_FRAME);
    return;
  }

  new ShareServiceImpl(*render_frame_host, std::move(receiver));
}

// static
bool ShareServiceImpl::IsDangerousFilename(const base::FilePath& path) {
  constexpr const base::FilePath::CharType* kPermitted[] = {
      FILE_PATH_LITERAL(".avif"),   // image/avif
      FILE_PATH_LITERAL(".bmp"),    // image/bmp / image/x-ms-bmp
      FILE_PATH_LITERAL(".css"),    // text/css
      FILE_PATH_LITERAL(".csv"),    // text/csv / text/comma-separated-values
      FILE_PATH_LITERAL(".ehtml"),  // text/html
      FILE_PATH_LITERAL(".flac"),   // audio/flac
      FILE_PATH_LITERAL(".gif"),    // image/gif
      FILE_PATH_LITERAL(".htm"),    // text/html
      FILE_PATH_LITERAL(".html"),   // text/html
      FILE_PATH_LITERAL(".ico"),    // image/x-icon
      FILE_PATH_LITERAL(".jfif"),   // image/jpeg
      FILE_PATH_LITERAL(".jpeg"),   // image/jpeg
      FILE_PATH_LITERAL(".jpg"),    // image/jpeg
      FILE_PATH_LITERAL(".m4a"),    // audio/x-m4a
      FILE_PATH_LITERAL(".m4v"),    // video/mp4
      FILE_PATH_LITERAL(".mp3"),    // audio/mpeg audio/mp3
      FILE_PATH_LITERAL(".mp4"),    // video/mp4
      FILE_PATH_LITERAL(".mpeg"),   // video/mpeg
      FILE_PATH_LITERAL(".mpg"),    // video/mpeg
      FILE_PATH_LITERAL(".oga"),    // audio/ogg
      FILE_PATH_LITERAL(".ogg"),    // audio/ogg
      FILE_PATH_LITERAL(".ogm"),    // video/ogg
      FILE_PATH_LITERAL(".ogv"),    // video/ogg
      FILE_PATH_LITERAL(".opus"),   // audio/ogg
      FILE_PATH_LITERAL(".pdf"),    // application/pdf
      FILE_PATH_LITERAL(".pjp"),    // image/jpeg
      FILE_PATH_LITERAL(".pjpeg"),  // image/jpeg
      FILE_PATH_LITERAL(".png"),    // image/png
      FILE_PATH_LITERAL(".shtm"),   // text/html
      FILE_PATH_LITERAL(".shtml"),  // text/html
      FILE_PATH_LITERAL(".svg"),    // image/svg+xml
      FILE_PATH_LITERAL(".svgz"),   // image/svg+xml
      FILE_PATH_LITERAL(".text"),   // text/plain
      FILE_PATH_LITERAL(".tif"),    // image/tiff
      FILE_PATH_LITERAL(".tiff"),   // image/tiff
      FILE_PATH_LITERAL(".txt"),    // text/plain
      FILE_PATH_LITERAL(".wav"),    // audio/wav
      FILE_PATH_LITERAL(".weba"),   // audio/webm
      FILE_PATH_LITERAL(".webm"),   // video/webm
      FILE_PATH_LITERAL(".webp"),   // image/webp
      FILE_PATH_LITERAL(".xbm"),    // image/x-xbitmap
  };

  for (const base::FilePath::CharType* permitted : kPermitted) {
    if (base::EndsWith(path.value(), permitted,
                       base::CompareCase::INSENSITIVE_ASCII))
      return false;
  }
  return true;
}

// static
bool ShareServiceImpl::IsDangerousMimeType(std::string_view content_type) {
  constexpr std::array<const char*, 28> kPermitted = {
      "application/pdf",
      "audio/flac",
      "audio/mp3",
      "audio/mpeg",
      "audio/ogg",
      "audio/wav",
      "audio/webm",
      "audio/x-m4a",
      "image/avif",
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
  UMA_HISTOGRAM_ENUMERATION(kWebShareApiCountMetric, WebShareMethod::kShare);

  if (!render_frame_host().IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kWebShare)) {
    std::move(callback).Run(blink::mojom::ShareError::PERMISSION_DENIED);
    ReportBadMessageAndDeleteThis("Feature policy blocks Web Share");
    return;
  }

  content::WebContents* const web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
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

  bool should_check_url = false;
  for (auto& file : files) {
    if (!file || !file->blob || !file->blob->blob) {
      mojo::ReportBadMessage("Invalid file to share()");
      return;
    }

    const base::FilePath& path = file->name.path();
    if (IsDangerousFilename(path) ||
        IsDangerousMimeType(file->blob->content_type)) {
      VLOG(1) << "File type is not supported: " << path << " has mime type "
              << file->blob->content_type;
      std::move(callback).Run(blink::mojom::ShareError::PERMISSION_DENIED);
      return;
    }

    // Check if at least one file is marked by the download protection service
    // to send a ping to check this file type.
    if (!should_check_url &&
        safe_browsing::FileTypePolicies::GetInstance()->IsCheckedBinaryFile(
            path)) {
      should_check_url = true;
    }

    // In the case where the original blob handle was to a native file (of
    // unknown size), the serialized data does not contain an accurate file
    // size. To handle this, the comparison against kMaxSharedFileBytes should
    // be done by the platform-specific implementations as part of processing
    // the blobs.
  }

  DCHECK(!safe_browsing_request_);
  if (should_check_url && g_browser_process->safe_browsing_service()) {
    safe_browsing_request_.emplace(
        g_browser_process->safe_browsing_service()->database_manager(),
        web_contents->GetLastCommittedURL(),
        base::BindOnce(&ShareServiceImpl::OnSafeBrowsingResultReceived,
                       weak_factory_.GetWeakPtr(), title, text, share_url,
                       std::move(files), std::move(callback)));
    return;
  }

  OnSafeBrowsingResultReceived(title, text, share_url, std::move(files),
                               std::move(callback),
                               /*is_url_safe=*/true);
}

void ShareServiceImpl::OnSafeBrowsingResultReceived(
    const std::string& title,
    const std::string& text,
    const GURL& share_url,
    std::vector<blink::mojom::SharedFilePtr> files,
    ShareCallback callback,
    bool is_url_safe) {
  safe_browsing_request_.reset();

  content::WebContents* const web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    VLOG(1) << "Cannot share after navigating away";
    std::move(callback).Run(blink::mojom::ShareError::PERMISSION_DENIED);
    return;
  }

  if (!is_url_safe) {
    VLOG(1) << "File not safe to share from this website";
    std::move(callback).Run(blink::mojom::ShareError::PERMISSION_DENIED);
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  sharesheet_client_.Share(title, text, share_url, std::move(files),
                           std::move(callback));
#elif BUILDFLAG(IS_MAC)
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
#elif BUILDFLAG(IS_WIN)
  auto share_operation = std::make_unique<webshare::ShareOperation>(
      title, text, share_url, std::move(files), web_contents);
  auto* const share_operation_ptr = share_operation.get();
  share_operation_ptr->Run(base::BindOnce(
      [](std::unique_ptr<webshare::ShareOperation> share_operation,
         ShareCallback callback,
         blink::mojom::ShareError result) { std::move(callback).Run(result); },
      std::move(share_operation), std::move(callback)));
#else
  NOTREACHED_IN_MIGRATION();
  std::move(callback).Run(blink::mojom::ShareError::INTERNAL_ERROR);
#endif
}
