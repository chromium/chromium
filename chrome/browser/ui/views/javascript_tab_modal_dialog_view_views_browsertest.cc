// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/javascript_tab_modal_dialog_view_views.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"

using JavaScriptTabModalDialogViewViewsBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(JavaScriptTabModalDialogViewViewsBrowserTest,
                       AlertDialogAccessibleNameDescriptionAndRole) {
  std::u16string title = u"Title";
  std::u16string message = u"The message";
  auto* dialog_views =
      JavaScriptTabModalDialogViewViews::CreateAlertDialogForTesting(
          browser(), title, message);

  // The JavaScriptTabModalDialogViewViews should set the RootView's accessible
  // name to the alert's title and the accessible description to the alert's
  // message text. The role of the RootView should be dialog.
  ui::AXNodeData data;
  dialog_views->GetWidget()
      ->GetRootView()
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            title);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            message);
  EXPECT_EQ(data.role, ax::mojom::Role::kDialog);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kDescriptionFrom),
            static_cast<int32_t>(ax::mojom::DescriptionFrom::kAriaDescription));
}

IN_PROC_BROWSER_TEST_F(JavaScriptTabModalDialogViewViewsBrowserTest,
                       AlertDialogCloseButtonAccessibilityIgnored) {
  std::u16string title = u"Title";
  std::u16string message = u"The message";
  auto* dialog_views =
      JavaScriptTabModalDialogViewViews::CreateAlertDialogForTesting(
          browser(), title, message);

  // In an alert, the Close button is not used. It should be removed from the
  // accessibility tree ("ignored").
  auto* bubble_frame_view = static_cast<views::BubbleFrameView*>(
      dialog_views->GetWidget()->non_client_view()->frame_view());
  if (auto* close_button = bubble_frame_view->close_button()) {
    EXPECT_TRUE(close_button->GetViewAccessibility().GetIsIgnored());
  }
}
