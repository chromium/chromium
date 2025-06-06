// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/window/dialog_delegate.h"

namespace tabs {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWidgetContentsViewElementId);

std::unique_ptr<views::Widget> CreateWidgetWithNoNonClientView() {
  auto content_view = std::make_unique<views::View>();
  content_view->SetPreferredSize(gfx::Size(500, 500));
  content_view->SetBackground(views::CreateSolidBackground(SK_ColorBLUE));

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::Ownership::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::Type::TYPE_WINDOW_FRAMELESS);
  widget_params.bounds = gfx::Rect({0, 0}, content_view->GetPreferredSize());

  auto widget = std::make_unique<views::Widget>();
  widget->Init(std::move(widget_params));
  CHECK_EQ(widget->non_client_view(), nullptr);

  content_view->SetProperty(views::kElementIdentifierKey,
                            kWidgetContentsViewElementId);
  widget->SetContentsView(std::move(content_view));
  return widget;
}

}  // namespace

class TabDialogManagerBrowserTest : public InteractiveBrowserTest {
 public:
  TabDialogManagerBrowserTest() = default;
  ~TabDialogManagerBrowserTest() override = default;

  TabDialogManagerBrowserTest(const TabDialogManagerBrowserTest&) = delete;
  TabDialogManagerBrowserTest& operator=(const TabDialogManagerBrowserTest&) =
      delete;

 protected:
  TabDialogManager* GetTabDialogManager() {
    TabInterface* tab_interface = browser()->GetActiveTabInterface();
    CHECK(tab_interface);
    return tab_interface->GetTabFeatures()->tab_dialog_manager();
  }
};

// Tests that a widget that does not have a non-client view can be shown without
// crashing.
IN_PROC_BROWSER_TEST_F(TabDialogManagerBrowserTest,
                       ShowWidgetThatHasNoNonClientView) {
  // Declare widget here to ensure its lifetime spans the RunTestSequence.
  std::unique_ptr<views::Widget> widget;

  RunTestSequence(
      Do([&]() { widget = CreateWidgetWithNoNonClientView(); }),
      Do([&, this]() {
        TabDialogManager* manager = GetTabDialogManager();
        manager->ShowDialog(widget.get(),
                            std::make_unique<tabs::TabDialogManager::Params>());
      }),
      InAnyContext(WaitForShow(kWidgetContentsViewElementId)),
      CheckResult([&]() { return widget && widget->IsVisible(); }, true,
                  "Verify widget is visible"));
}

}  // namespace tabs
