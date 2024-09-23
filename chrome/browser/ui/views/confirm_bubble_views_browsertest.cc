// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/confirm_bubble_views.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/confirm_bubble_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/test/browser_test.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace {

static const std::u16string& kDialogTitle = u"Test dialog";
static const std::u16string& kMessageText =
    u"A very long message which should be forced to wrap when displayed "
    u"in the confirm bubble; this can be used to verify proper "
    u"positioning of text with respect to the bubble bounds and other "
    u"elements.";

class ConfirmBubbleTestModel : public ConfirmBubbleModel {
  std::u16string GetTitle() const override { return kDialogTitle; }
  std::u16string GetMessageText() const override { return kMessageText; }
};

}  // namespace

class ConfirmBubbleTest : public DialogBrowserTest {
 public:
  ConfirmBubbleTest() = default;
  ConfirmBubbleTest(const ConfirmBubbleTest&) = delete;
  ConfirmBubbleTest& operator=(const ConfirmBubbleTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    constrained_window::CreateBrowserModalDialogViews(
        new ConfirmBubbleViews(std::make_unique<ConfirmBubbleTestModel>()),
        browser()->window()->GetNativeWindow())
        ->Show();
  }
};

IN_PROC_BROWSER_TEST_F(ConfirmBubbleTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ConfirmBubbleTest, RootViewAccessibleProperties) {
  auto* confirm_bubble_views =
      new ConfirmBubbleViews(std::make_unique<ConfirmBubbleTestModel>());
  constrained_window::CreateBrowserModalDialogViews(
      confirm_bubble_views, browser()->window()->GetNativeWindow())
      ->Show();

  auto* widget = confirm_bubble_views->GetWidget();
  DCHECK(widget);

  auto* root_view = widget->GetRootView();
  DCHECK(root_view);

  ui::AXNodeData data;
  root_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            kDialogTitle);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            kMessageText);

  // TODO(accessibility): The title is not yet being set as the accessible name
  // of `RootView`. As a result, `GetAccessibleName` will fail. The reason we
  // can use `GetViewAccessibility().GetCachedDescription`
  // successfully is because the description is set in
  // `ConfirmBubbleViews::OnWidgetInitialized`.
  EXPECT_EQ(root_view->GetViewAccessibility().GetCachedDescription(),
            kMessageText);
}
