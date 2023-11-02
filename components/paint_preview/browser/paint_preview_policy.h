// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_POLICY_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_POLICY_H_

namespace content {
class WebContents;
}

namespace paint_preview {

// Subclasses of PaintPreviewPolicy are responsible for determining whether a
// given WebContents are amenable for PaintPreview. For example, sites that make
// heavy use of script.
class PaintPreviewPolicy {
 public:
  virtual ~PaintPreviewPolicy() = default;
  virtual bool SupportedForContents(content::WebContents* web_contents) = 0;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_POLICY_H_
