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
  if (!IsPdfInternalPluginAllowedOrigin(
          extension_host->GetLastCommittedOrigin())) {
    return nullptr;
  }

  content::RenderFrameHost* pdf_rfh = nullptr;
  extension_host->ForEachRenderFrameHost(
      [&pdf_rfh](content::RenderFrameHost* rfh) {
        if (!rfh->GetProcess()->IsPdf()) {
          return;
        }

        DCHECK(IsPdfInternalPluginAllowedOrigin(
            rfh->GetParent()->GetLastCommittedOrigin()));
        DCHECK(!pdf_rfh);
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

}  // namespace pdf_frame_util
