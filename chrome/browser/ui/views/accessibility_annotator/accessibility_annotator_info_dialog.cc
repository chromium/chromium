// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility_annotator/accessibility_annotator_info_dialog.h"

#include <utility>

#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/webview/webview.h"

namespace accessibility_annotator::info {

namespace {
constexpr int kBubbleWidth = 448;
constexpr int kBubbleMaxHeight = 500;
}  // namespace

AccessibilityAnnotatorInfoDialog::AccessibilityAnnotatorInfoDialog(
    views::View* anchor_view,
    std::unique_ptr<WebUIContentsWrapper> contents_wrapper)
    : WebUIBubbleDialogView(anchor_view, contents_wrapper->GetWeakPtr()),
      contents_wrapper_(std::move(contents_wrapper)) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_fixed_width(kBubbleWidth);
  set_margins(gfx::Insets());

  if (web_view()) {
    web_view()->SetPreferredSize(gfx::Size(kBubbleWidth, kBubbleMaxHeight));
  }
}

AccessibilityAnnotatorInfoDialog::~AccessibilityAnnotatorInfoDialog() = default;

BEGIN_METADATA(AccessibilityAnnotatorInfoDialog)
END_METADATA

}  // namespace accessibility_annotator::info
