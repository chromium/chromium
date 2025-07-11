// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace tabs {

class TabDialogManagerDesktopWidgetUiTest : public InProcessBrowserTest {
 public:
  TabDialogManagerDesktopWidgetUiTest() {
    feature_list_.InitAndEnableFeature(features::kTabModalUsesDesktopWidget);
  }

  TabDialogManagerDesktopWidgetUiTest(
      const TabDialogManagerDesktopWidgetUiTest&) = delete;
  TabDialogManagerDesktopWidgetUiTest& operator=(
      const TabDialogManagerDesktopWidgetUiTest&) = delete;

 protected:
  TabDialogManager* GetTabDialogManager() {
    TabInterface* tab_interface = browser()->GetActiveTabInterface();
    CHECK(tab_interface);
    return tab_interface->GetTabFeatures()->tab_dialog_manager();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that when TabModalUsesDesktopWidget is enabled, the created widget
// is a desktop widget.
IN_PROC_BROWSER_TEST_F(TabDialogManagerDesktopWidgetUiTest,
                       CreatesDesktopWidget) {
  TabDialogManager* manager = GetTabDialogManager();
  ASSERT_TRUE(manager);

  ui::DialogModel::Builder dialog_builder;
  auto model_host = views::BubbleDialogModelHost::CreateModal(
                         dialog_builder.Build(), ui::mojom::ModalType::kChild);
  model_host->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  auto widget = manager->CreateAndShowDialog(
      model_host.release(), std::make_unique<tabs::TabDialogManager::Params>());

  EXPECT_TRUE(widget->GetIsDesktopWidget());
}

}  // namespace tabs
