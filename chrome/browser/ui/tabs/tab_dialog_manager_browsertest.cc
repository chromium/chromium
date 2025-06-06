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
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/models/dialog_model.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
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

std::unique_ptr<views::Widget> CreateWidgetWithDialogModel() {
  ui::DialogModel::Builder dialog_builder;
  dialog_builder.SetTitle(u"Test Dialog Model")
      .AddParagraph(ui::DialogModelLabel(u"This is a test dialog."), u"",
                    kWidgetContentsViewElementId)
      .AddOkButton(base::DoNothing(),
                   ui::DialogModel::Button::Params().SetLabel(u"OK"));
  auto dialog_model = dialog_builder.Build();

  auto bubble_delegate = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), /*anchor_view=*/nullptr,
      views::BubbleBorder::NONE);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::Ownership::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::Type::TYPE_BUBBLE);

  auto widget = std::make_unique<views::Widget>();
  // BubbleDialogModelHost is owned by the widget.
  widget_params.delegate = bubble_delegate.release();
  widget->Init(std::move(widget_params));

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

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();
  }
};

// Tests that a widget that does not have a non-client view can be shown without
// crashing.
IN_PROC_BROWSER_TEST_F(TabDialogManagerBrowserTest,
                       ShowWidgetThatHasNoNonClientView) {
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

IN_PROC_BROWSER_TEST_F(TabDialogManagerBrowserTest, ShowDialogModel) {
  std::unique_ptr<views::Widget> widget;

  RunTestSequence(
      Do([&]() { widget = CreateWidgetWithDialogModel(); }), Do([&, this]() {
        TabDialogManager* manager = GetTabDialogManager();
        manager->ShowDialog(widget.get(),
                            std::make_unique<tabs::TabDialogManager::Params>());
      }),
      InAnyContext(WaitForShow(kWidgetContentsViewElementId)),
      CheckResult([&]() { return widget->IsVisible(); }, true,
                  "Verify widget is visible"));
}

// Tests that the widget is closed on cross-site navigation if
// TabDialogManager::Params::close_on_navigate is true.
IN_PROC_BROWSER_TEST_F(TabDialogManagerBrowserTest,
                       Parmas_close_on_navigate_true_CrossSiteNavigation) {
  std::unique_ptr<views::Widget> widget_ptr;
  const GURL kInitialUrl =
      embedded_test_server()->GetURL("foo.com", "/title1.html");
  const GURL kDifferentSiteUrl =
      embedded_test_server()->GetURL("bar.com", "/title2.html");

  RunTestSequence(
      Do([&]() {
        ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInitialUrl));
      }),
      Do([&]() { widget_ptr = CreateWidgetWithNoNonClientView(); }),
      Do([&, this]() {
        TabDialogManager* manager = GetTabDialogManager();
        auto params = std::make_unique<tabs::TabDialogManager::Params>();
        params->close_on_navigate = true;
        manager->ShowDialog(widget_ptr.get(), std::move(params));
      }),
      InAnyContext(WaitForShow(kWidgetContentsViewElementId)),
      CheckResult([&]() { return widget_ptr && widget_ptr->IsVisible(); }, true,
                  "Verify widget is initially visible"),
      Do([&]() {
        ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kDifferentSiteUrl));
      }),
      InAnyContext(WaitForHide(kWidgetContentsViewElementId)));
}

// Tests that the widget is not closed on same-site navigation if
// TabDialogManager::Params::close_on_navigate is true.
IN_PROC_BROWSER_TEST_F(TabDialogManagerBrowserTest,
                       Parmas_close_on_navigate_true_SameSiteNavigation) {
  std::unique_ptr<views::Widget> widget_ptr;
  const GURL kInitialUrl =
      embedded_test_server()->GetURL("foo.com", "/title1.html");
  const GURL kSameSiteUrl =
      embedded_test_server()->GetURL("foo.com", "/title2.html");

  RunTestSequence(
      Do([&]() {
        ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInitialUrl));
      }),
      Do([&]() { widget_ptr = CreateWidgetWithNoNonClientView(); }),
      Do([&, this]() {
        TabDialogManager* manager = GetTabDialogManager();
        auto params = std::make_unique<tabs::TabDialogManager::Params>();
        params->close_on_navigate = true;
        manager->ShowDialog(widget_ptr.get(), std::move(params));
      }),
      InAnyContext(WaitForShow(kWidgetContentsViewElementId)),
      CheckResult([&]() { return widget_ptr && widget_ptr->IsVisible(); }, true,
                  "Verify widget is initially visible"),
      Do([&]() {
        ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kSameSiteUrl));
      }),
      InAnyContext(EnsurePresent(kWidgetContentsViewElementId)));
}

// Tests that the widget is not closed on cross-site navigation if
// TabDialogManager::Params::close_on_navigate is false.
IN_PROC_BROWSER_TEST_F(TabDialogManagerBrowserTest,
                       Parmas_close_on_navigate_false_CrossSiteNavigation) {
  std::unique_ptr<views::Widget> widget_ptr;
  const GURL kInitialUrl =
      embedded_test_server()->GetURL("foo.com", "/title1.html");
  const GURL kDifferentSiteUrl =
      embedded_test_server()->GetURL("bar.com", "/title2.html");

  RunTestSequence(
      Do([&]() {
        ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInitialUrl));
      }),
      Do([&]() { widget_ptr = CreateWidgetWithNoNonClientView(); }),
      Do([&, this]() {
        TabDialogManager* manager = GetTabDialogManager();
        auto params = std::make_unique<tabs::TabDialogManager::Params>();
        params->close_on_navigate = false;
        manager->ShowDialog(widget_ptr.get(), std::move(params));
      }),
      InAnyContext(WaitForShow(kWidgetContentsViewElementId)),
      CheckResult([&]() { return widget_ptr && widget_ptr->IsVisible(); }, true,
                  "Verify widget is initially visible"),
      Do([&]() {
        ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kDifferentSiteUrl));
      }),
      InAnyContext(EnsurePresent(kWidgetContentsViewElementId)));
}

}  // namespace tabs
