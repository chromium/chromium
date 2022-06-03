// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_TEST_PAINT_PREVIEW_POLICY_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_TEST_PAINT_PREVIEW_POLICY_H_

#include "components/paint_preview/browser/paint_preview_policy.h"

namespace content {
class WebContents;
}

namespace paint_preview {

// Simple implementation of PaintPreviewPolicy used in tests.
// SupportedForContents defaults to true, but tests may change
// this by calling SetSupportedForContents.
class TestPaintPreviewPolicy : public PaintPreviewPolicy {
 public:
  TestPaintPreviewPolicy();

  ~TestPaintPreviewPolicy() override;

  bool SupportedForContents(content::WebContents* web_contents) override;

  void SetSupportedForContents(bool supported_for_contents) {
    supported_for_contents_ = supported_for_contents;
  }

 private:
  bool supported_for_contents_ = true;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_TEST_PAINT_PREVIEW_POLICY_H_
