// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_first_content_paint_registry.h"

#include <utility>

#include "base/no_destructor.h"

namespace pdf {

namespace {

PdfFirstContentPaintCallbackList& GetCallbackList() {
  static base::NoDestructor<PdfFirstContentPaintCallbackList> callbacks;
  return *callbacks;
}

}  // namespace

base::CallbackListSubscription RegisterPdfFirstContentPaintCallback(
    PdfFirstContentPaintCallbackList::CallbackType callback) {
  return GetCallbackList().Add(std::move(callback));
}

void NotifyPdfFirstContentPainted(content::WebContents* embedder,
                                  base::TimeTicks paint_time) {
  GetCallbackList().Notify(embedder, paint_time);
}

}  // namespace pdf
