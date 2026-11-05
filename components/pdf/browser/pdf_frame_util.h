// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_PDF_FRAME_UTIL_H_
#define COMPONENTS_PDF_BROWSER_PDF_FRAME_UTIL_H_

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

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

// Returns the origin of the PDF document hosted in `pdf_content_frame`, i.e.
// the origin committed by the PDF's placeholder document: the document in the
// frame that navigated to the PDF URL, which embeds the PDF viewer. Use this
// instead of `pdf_content_frame`'s own committed origin when the origin the PDF
// was loaded from is needed: with `features::kPdfOpaqueOrigin`, the PDF content
// frame commits an opaque origin as an implementation detail of how PDF content
// is isolated (see https://crbug.com/559567801).
//
// Intended for PDFs shown in the PDF viewer (either the OOPIF or GuestView
// version). For other PDF content frames, such as Print Preview's, this returns
// the frame's own committed origin, which is opaque with
// `features::kPdfOpaqueOrigin`.
url::Origin GetPdfOrigin(const content::RenderFrameHost& pdf_content_frame);

}  // namespace pdf_frame_util

#endif  // COMPONENTS_PDF_BROWSER_PDF_FRAME_UTIL_H_
