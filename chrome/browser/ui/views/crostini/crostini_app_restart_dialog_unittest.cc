// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_app_restart_dialog.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

class CrostiniAppRestartDialogTest : public ChromeViewsTestBase {
 public:
  CrostiniAppRestartDialogTest() = default;
  ~CrostiniAppRestartDialogTest() override = default;

  views::test::WidgetTest::WidgetAutoclosePtr ShowDialog() {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "CrostiniAppRestart");
    crostini::ShowAppRestartDialogForTesting(GetContext());
    return views::test::WidgetTest::WidgetAutoclosePtr(
        waiter.WaitIfNeededAndGet());
  }
};

TEST_F(CrostiniAppRestartDialogTest, OnlyHasOkButton) {
  auto widget = ShowDialog();
  EXPECT_EQ(widget->widget_delegate()->AsDialogDelegate()->buttons(),
            static_cast<int>(ui::mojom::DialogButton::kOk));
}

TEST_F(CrostiniAppRestartDialogTest, IsSystemModal) {
  auto widget = ShowDialog();
  EXPECT_EQ(widget->widget_delegate()->AsDialogDelegate()->GetModalType(),
            ui::mojom::ModalType::kSystem);
}

TEST_F(CrostiniAppRestartDialogTest, ContentsViewHasModalPreferredWidth) {
  auto widget = ShowDialog();
  EXPECT_EQ(widget->widget_delegate()->GetContentsView()->width(),
            ChromeLayoutProvider::Get()->GetDistanceMetric(
                views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
}
