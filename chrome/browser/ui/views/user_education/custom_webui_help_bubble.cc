// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/custom_webui_help_bubble.h"

#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"

CustomWebUIHelpBubble::~CustomWebUIHelpBubble() = default;

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CustomWebUIHelpBubble,
                                      kHelpBubbleIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CustomWebUIHelpBubble,
                                      kWebViewIdForTesting);

// static
CustomWebUIHelpBubble::BuildCustomWebUIHelpBubbleViewCallback
CustomWebUIHelpBubble::GetDefaultBuildBubbleViewCallback() {
  return base::BindRepeating(
      [](views::View* anchor_view, views::BubbleBorder::Arrow arrow,
         base::WeakPtr<WebUIContentsWrapper> contents_wrapper) {
        auto result = std::make_unique<WebUIBubbleDialogView>(
            anchor_view, std::move(contents_wrapper), std::nullopt, arrow);
        result->SetProperty(views::kElementIdentifierKey,
                            kHelpBubbleIdForTesting);
        result->web_view()->SetProperty(views::kElementIdentifierKey,
                                        kWebViewIdForTesting);
        return result;
      });
}
