// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_frame_util.h"

#include <functional>

#include "base/check.h"
#include "base/functional/bind.h"
#include "components/pdf/common/constants.h"
#include "components/pdf/common/pdf_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "pdf/pdf_features.h"
#include "url/origin.h"

namespace pdf_frame_util {

content::RenderFrameHost* FindFullPagePdfExtensionHost(
    content::WebContents* contents) {
  CHECK(chrome_pdf::features::IsOopifPdfEnabled());

  // MIME type associated with `contents` must be `application/pdf` for a
  // full-page PDF.
  if (contents->GetContentsMimeType() != pdf::kPDFMimeType) {
    return nullptr;
  }

  // A full-page PDF embedder host should have a child PDF extension host.
  content::RenderFrameHost* extension_host = nullptr;
  contents->GetPrimaryMainFrame()->ForEachRenderFrameHostWithAction(
      [&extension_host](content::RenderFrameHost* child_host) {
        content::RenderFrameHost* parent_host = child_host->GetParent();
        // Don't descend past the first level of children (extension frame must
        // be a direct child to main frame).
        if (parent_host && !parent_host->IsInPrimaryMainFrame()) {
          return content::RenderFrameHost::FrameIterationAction::kSkipChildren;
        }

        if (IsPdfExtensionOrigin(child_host->GetLastCommittedOrigin())) {
          CHECK(!extension_host);
          extension_host = child_host;
        }

        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });

  return extension_host;
}

content::RenderFrameHost* FindPdfChildFrame(
    content::RenderFrameHost* extension_host) {
  if (!IsPdfExtensionOrigin(extension_host->GetLastCommittedOrigin())) {
    return nullptr;
  }

  content::RenderFrameHost* pdf_rfh = nullptr;
  extension_host->ForEachRenderFrameHost(
      [&pdf_rfh](content::RenderFrameHost* rfh) {
        if (!rfh->GetProcess()->IsPdf()) {
          return;
        }

        CHECK(IsPdfExtensionOrigin(rfh->GetParent()->GetLastCommittedOrigin()));
        CHECK(!pdf_rfh);
        pdf_rfh = rfh;
      });

  return pdf_rfh;
}

content::RenderFrameHost* GetEmbedderHost(
    content::RenderFrameHost* content_host) {
  CHECK(chrome_pdf::features::IsOopifPdfEnabled());

  if (!content_host) {
    return nullptr;
  }

  content::RenderFrameHost* extension_host = content_host->GetParent();
  if (!extension_host ||
      !IsPdfExtensionOrigin(extension_host->GetLastCommittedOrigin())) {
    return nullptr;
  }

  return extension_host->GetParent();
}

url::Origin GetPdfOrigin(const content::RenderFrameHost& pdf_content_frame) {
  // Look up the embedder frame's origin rather than using the content frame's
  // own origin. With `features::kPdfOpaqueOrigin`, the PDF content frame
  // commits an opaque origin (with the PDF's origin as its precursor), so that
  // PDF content can't access data belonging to the PDF's origin (see
  // https://crbug.com/559567801). That opaque origin is an implementation
  // detail of PDF isolation, and callers that want to know where the PDF came
  // from should not see it. The embedder frame is the placeholder document
  // that commits the PDF URL normally (its content is replaced with markup that
  // hosts the PDF viewer), so its origin is the origin the PDF content frame
  // would have had without `features::kPdfOpaqueOrigin`. Unlike unwrapping the
  // opaque origin's precursor, this also preserves origins that would have been
  // opaque anyway.
  //
  // The PDF content frame is a child of the PDF extension frame. For the OOPIF
  // PDF viewer, the extension frame is a subframe of the embedder. For the
  // GuestView PDF viewer, the extension frame is the main frame of the
  // MimeHandlerViewGuest, and escaping the GuestView leads to the guest's
  // owner, which is the embedder.
  content::RenderFrameHost* extension_host = pdf_content_frame.GetParent();
  content::RenderFrameHost* embedder_host =
      extension_host &&
              IsPdfExtensionOrigin(extension_host->GetLastCommittedOrigin())
          ? extension_host->GetParentOrOuterDocumentOrEmbedder()
          : nullptr;
  if (embedder_host) {
    return embedder_host->GetLastCommittedOrigin();
  }

  // PDF content that isn't hosted by the PDF viewer has no PDF extension frame
  // to walk from. For example, Print Preview's PDF content frame is a
  // chrome-untrusted://print child of the chrome://print WebUI. Fall back to
  // the content frame's own origin in that case. Note that with
  // `features::kPdfOpaqueOrigin`, that origin is opaque for Print Preview too,
  // as for any other PDF content frame.
  return pdf_content_frame.GetLastCommittedOrigin();
}

}  // namespace pdf_frame_util
