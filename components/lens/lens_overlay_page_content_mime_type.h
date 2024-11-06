// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_OVERLAY_PAGE_CONTENT_MIME_TYPE_H_
#define COMPONENTS_LENS_LENS_OVERLAY_PAGE_CONTENT_MIME_TYPE_H_

namespace lens {

// The possible page content types for a contextual query.
enum class PageContentMimeType {
  kNone = 0,
  kPdf = 1,
  kHtml = 2,
  kPlainText = 3,
};

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_OVERLAY_PAGE_CONTENT_MIME_TYPE_H_
