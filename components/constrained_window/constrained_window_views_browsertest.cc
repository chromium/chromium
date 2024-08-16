// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/constrained_window/constrained_window_views.h"

#include <memory>

#include "build/build_config.h"
#include "components/web_modal/test_web_contents_modal_dialog_host.h"
#include "components/web_modal/test_web_contents_modal_dialog_manager_delegate.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace constrained_window {

class ConstrainedWindowViewsBrowserTest : public content::ContentBrowserTest {
 public:
  void PreRunTestOnMainThread() override {
    content::ContentBrowserTest::PreRunTestOnMainThread();

    content::WebContents* web_contents = shell()->web_contents();

    dialog_delegate_ = std::make_unique<
        web_modal::TestWebContentsModalDialogManagerDelegate>();
    dialog_host_ = std::make_unique<web_modal::TestWebContentsModalDialogHost>(
        web_contents->GetNativeView());
    dialog_delegate_->set_web_contents_modal_dialog_host(dialog_host_.get());

    web_modal::WebContentsModalDialogManager::CreateForWebContents(
        web_contents);
    web_modal::WebContentsModalDialogManager::FromWebContents(web_contents)
        ->SetDelegate(dialog_delegate_.get());
  }

 private:
  std::unique_ptr<web_modal::TestWebContentsModalDialogHost> dialog_host_;
  std::unique_ptr<web_modal::TestWebContentsModalDialogManagerDelegate>
      dialog_delegate_;
};

// Test that DialogDelegate::SetOwnershipOfNewWidget() controls the ownership of
// the views::Widget created by ShowMebModalDialogViewsOwned().
IN_PROC_BROWSER_TEST_F(ConstrainedWindowViewsBrowserTest,
                       ShowWebModalDialogViewsOwned) {
  auto delegate = std::make_unique<views::DialogDelegate>();
  delegate->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  delegate->SetModalType(ui::mojom::ModalType::kChild);

  std::unique_ptr<views::Widget> widget = ShowWebModalDialogViewsOwned(
      delegate.get(), shell()->web_contents(),
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  EXPECT_EQ(widget->ownership(), views::Widget::InitParams::CLIENT_OWNS_WIDGET);
}

}  // namespace constrained_window
