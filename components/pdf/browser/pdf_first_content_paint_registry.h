// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_PDF_FIRST_CONTENT_PAINT_REGISTRY_H_
#define COMPONENTS_PDF_BROWSER_PDF_FIRST_CONTENT_PAINT_REGISTRY_H_

#include "base/callback_list.h"
#include "base/time/time.h"

namespace content {
class WebContents;
}  // namespace content

namespace pdf {

// Broadcasts "the PDF plugin finished rendering document content for the first
// time".
//
// This is a process-wide list rather than an observer on PDFDocumentHelper
// because of ordering. The helper is a DocumentUserData that only exists once
// the PDF renderer binds its PdfHost, which happens well after the tab is known
// to be a PDF and after any startup profiler observing that tab is created. A
// subscriber that had to wait for the helper to exist would have to poll for
// it; subscribing here cannot race because the list outlives every helper.
//
// `embedder` is the WebContents responsible for displaying the PDF - the tab -
// under both the MimeHandlerView guest architecture and OOPIF, so a subscriber
// can match it against a tab it cares about without knowing which architecture
// is in use.
using PdfFirstContentPaintCallbackList =
    base::RepeatingCallbackList<void(content::WebContents* embedder,
                                     base::TimeTicks paint_time)>;

// Subscribes for as long as the returned subscription is held.
[[nodiscard]] base::CallbackListSubscription
RegisterPdfFirstContentPaintCallback(
    PdfFirstContentPaintCallbackList::CallbackType callback);

// Called by PDFDocumentHelper when the plugin reports its first content paint.
void NotifyPdfFirstContentPainted(content::WebContents* embedder,
                                  base::TimeTicks paint_time);

}  // namespace pdf

#endif  // COMPONENTS_PDF_BROWSER_PDF_FIRST_CONTENT_PAINT_REGISTRY_H_
