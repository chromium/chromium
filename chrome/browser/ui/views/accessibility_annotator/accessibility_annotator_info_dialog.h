// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_INFO_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_INFO_DIALOG_H_

#include <memory>

#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

class WebUIContentsWrapper;

namespace accessibility_annotator::info {

class AccessibilityAnnotatorInfoDialog : public WebUIBubbleDialogView {
  METADATA_HEADER(AccessibilityAnnotatorInfoDialog, WebUIBubbleDialogView)

 public:
  AccessibilityAnnotatorInfoDialog(
      views::View* anchor_view,
      std::unique_ptr<WebUIContentsWrapper> contents_wrapper);
  AccessibilityAnnotatorInfoDialog(const AccessibilityAnnotatorInfoDialog&) =
      delete;
  AccessibilityAnnotatorInfoDialog& operator=(
      const AccessibilityAnnotatorInfoDialog&) = delete;
  ~AccessibilityAnnotatorInfoDialog() override;

 private:
  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;
};

}  // namespace accessibility_annotator::info

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_INFO_DIALOG_H_
