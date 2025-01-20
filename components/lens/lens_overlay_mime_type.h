// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_OVERLAY_MIME_TYPE_H_
#define COMPONENTS_LENS_LENS_OVERLAY_MIME_TYPE_H_

namespace lens {

// An enum to represent MIME types that the Lens Overlay can be initialized on.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(LensOverlayMimeType)
enum class MimeType {
  kUnknown = 0,    // The document type is not one of the below types.
  kPdf = 1,        // "application/pdf"
  kHtml = 2,       // "text/html"
  kPlainText = 3,  // "text/plain"
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/lens/enums.xml:LensOverlayMimeType)
// When adding a value here, also update:
// - tools/metrics/histograms/metadata/lens/histogram.xml: <variants
// name="MimeTypes">
}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_OVERLAY_MIME_TYPE_H_
