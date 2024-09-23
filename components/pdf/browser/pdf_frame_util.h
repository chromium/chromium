// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_PDF_FRAME_UTIL_H_
#define COMPONENTS_PDF_BROWSER_PDF_FRAME_UTIL_H_

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace pdf_frame_util {

// For full-page OOPIF PDF viewer only. Searches the children of the primary
// main frame of `contents` to find a `RenderFrameHost` that hosts the PDF
// extension. Full-page PDF viewers must have only one PDF extension host.
content::RenderFrameHost* FindFullPagePdfExtensionHost(
    content::WebContents* contents);

// Searches the children of the given `extension_host` to find a
// `RenderFrameHost` that hosts PDF content.
content::RenderFrameHost* FindPdfChildFrame(
    content::RenderFrameHost* extension_host);

// For OOPIF PDF viewer only. Gets the embedder `RenderFrameHost` given the PDF
// content host. Returns nullptr if `content_host` is an invalid PDF content
// host.
content::RenderFrameHost* GetEmbedderHost(
    content::RenderFrameHost* content_host);

}  // namespace pdf_frame_util

#endif  // COMPONENTS_PDF_BROWSER_PDF_FRAME_UTIL_H_
