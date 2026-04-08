// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDialog1Id);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDialog2Id);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab1Id);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab2Id);

class NativeWidgetMacNSWindowInteractiveUITest : public InteractiveBrowserTest {
 public:
  NativeWidgetMacNSWindowInteractiveUITest() = default;

  NativeWidgetMacNSWindowInteractiveUITest(
      const NativeWidgetMacNSWindowInteractiveUITest&) = delete;
  NativeWidgetMacNSWindowInteractiveUITest& operator=(
      const NativeWidgetMacNSWindowInteractiveUITest&) = delete;

 protected:
  std::unique_ptr<views::Widget> CreateAndShowTestDialog(
      ui::ElementIdentifier identifier) {
    ui::DialogModel::Builder dialog_builder;
    dialog_builder.SetInternalName("TestDialog");
    dialog_builder.AddParagraph(ui::DialogModelLabel(u"Test"), u"", identifier);

    tabs::TabDialogManager* manager = GetTabDialogManager();
    CHECK(manager);

    auto model_host = views::BubbleDialogModelHost::CreateModal(
        dialog_builder.Build(), ui::mojom::ModalType::kChild);
    model_host->SetOwnershipOfNewWidget(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET);

    return manager->CreateAndShowDialog(
        model_host.release(),
        std::make_unique<tabs::TabDialogManager::Params>());
  }

  tabs::TabDialogManager* GetTabDialogManager() {
    tabs::TabInterface* tab_interface = browser()->GetActiveTabInterface();
    CHECK(tab_interface);
    return tab_interface->GetTabFeatures()->tab_dialog_manager();
  }
};

IN_PROC_BROWSER_TEST_F(NativeWidgetMacNSWindowInteractiveUITest,
                       ChildModalPreventsParentFromBecomingKey) {
  views::Widget* browser_widget = nullptr;
  std::unique_ptr<views::Widget> modal_widget;
  NSWindow* browser_window = nullptr;
  NSWindow* modal_window = nullptr;

  RunTestSequence(
      Do([&]() {
        browser_widget =
            BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
        browser_window = browser_widget->GetNativeWindow().GetNativeNSWindow();
      }),
      PollUntil([&]() -> bool { return [browser_window isKeyWindow]; },
                "Wait for browser window to be key"),
      Do([&]() { modal_widget = CreateAndShowTestDialog(kDialog1Id); }),
      WaitForShow(kDialog1Id), Do([&]() {
        modal_window = modal_widget->GetNativeWindow().GetNativeNSWindow();
      }),
      PollUntil(
          [&]() {
            return [modal_window isKeyWindow] &&
                   [browser_window.childWindows containsObject:modal_window];
          },
          "Wait for modal window to be key and child"),
      // Click the web contents scrim. The modal should remain key.
      MoveMouseTo(ContentsWebView::kContentsWebViewElementId), ClickMouse(),
      PollUntil([&]() -> bool { return [modal_window isKeyWindow]; },
                "Wait for modal window to stay key"),
      Check([&]() { return ![browser_window isKeyWindow]; }));
}

IN_PROC_BROWSER_TEST_F(NativeWidgetMacNSWindowInteractiveUITest,
                       ClickOnBrowserUIRemovesKeyFromChildModal) {
  views::Widget* browser_widget = nullptr;
  std::unique_ptr<views::Widget> modal_widget;
  NSWindow* browser_window = nullptr;
  NSWindow* modal_window = nullptr;

  RunTestSequence(
      Do([&]() {
        browser_widget =
            BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
        browser_window = browser_widget->GetNativeWindow().GetNativeNSWindow();
      }),
      PollUntil([&]() -> bool { return [browser_window isKeyWindow]; },
                "Wait for browser window to be key"),
      Do([&]() { modal_widget = CreateAndShowTestDialog(kDialog1Id); }),
      WaitForShow(kDialog1Id), Do([&]() {
        modal_window = modal_widget->GetNativeWindow().GetNativeNSWindow();
      }),
      PollUntil(
          [&]() {
            return [modal_window isKeyWindow] &&
                   [browser_window.childWindows containsObject:modal_window];
          },
          "Wait for modal window to be key and child"),
      // Click the omnibox. The browser window should become key.
      MoveMouseTo(kOmniboxElementId), ClickMouse(),
      PollUntil([&]() -> bool { return [browser_window isKeyWindow]; },
                "Wait for browser window to become key"),
      Check([&]() { return ![modal_window isKeyWindow]; }),
      CheckViewProperty(kOmniboxElementId, &views::View::HasFocus, true));
}

IN_PROC_BROWSER_TEST_F(NativeWidgetMacNSWindowInteractiveUITest,
                       ClickOnScrimAfterBrowserUIFocusReturnsKeyToChildModal) {
  views::Widget* browser_widget = nullptr;
  std::unique_ptr<views::Widget> modal_widget;
  NSWindow* browser_window = nullptr;
  NSWindow* modal_window = nullptr;

  RunTestSequence(
      Do([&]() {
        browser_widget =
            BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
        browser_window = browser_widget->GetNativeWindow().GetNativeNSWindow();
      }),
      PollUntil([&]() -> bool { return [browser_window isKeyWindow]; },
                "Wait for browser window to be key"),
      Do([&]() { modal_widget = CreateAndShowTestDialog(kDialog1Id); }),
      WaitForShow(kDialog1Id), Do([&]() {
        modal_window = modal_widget->GetNativeWindow().GetNativeNSWindow();
      }),
      PollUntil(
          [&]() {
            return [modal_window isKeyWindow] &&
                   [browser_window.childWindows containsObject:modal_window];
          },
          "Wait for modal window to be key and child"),
      // Click the omnibox. The browser window should become key.
      MoveMouseTo(kOmniboxElementId), ClickMouse(),
      PollUntil([&]() -> bool { return [browser_window isKeyWindow]; },
                "Wait for browser window to become key"),
      Check([&]() { return ![modal_window isKeyWindow]; }),
      CheckViewProperty(kOmniboxElementId, &views::View::HasFocus, true),
      // Click the web contents scrim. The modal should become key again.
      MoveMouseTo(ContentsWebView::kContentsWebViewElementId), ClickMouse(),
      PollUntil([&]() -> bool { return [modal_window isKeyWindow]; },
                "Wait for modal window to become key"),
      Check([&]() { return ![browser_window isKeyWindow]; }));
}

IN_PROC_BROWSER_TEST_F(
    NativeWidgetMacNSWindowInteractiveUITest,
    ClickOnScrimWithMultipleTabsAndModalsReturnsKeyToActiveModal) {
  views::Widget* browser_widget = nullptr;
  std::unique_ptr<views::Widget> modal_widget1;
  std::unique_ptr<views::Widget> modal_widget2;
  NSWindow* browser_window = nullptr;
  NSWindow* modal_window1 = nullptr;
  NSWindow* modal_window2 = nullptr;

  RunTestSequence(
      Do([&]() {
        browser_widget =
            BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
        browser_window = browser_widget->GetNativeWindow().GetNativeNSWindow();
      }),
      // Open two tabs
      AddInstrumentedTab(kTab2Id, GURL(url::kAboutBlankURL)),
      InstrumentTab(kTab1Id, 0),
      // Show a web-modal in Tab 1
      SelectTab(kTabStripElementId, 0),
      Do([&]() { modal_widget1 = CreateAndShowTestDialog(kDialog1Id); }),
      WaitForShow(kDialog1Id), Do([&]() {
        modal_window1 = modal_widget1->GetNativeWindow().GetNativeNSWindow();
      }),
      // Show a web-modal in Tab 2
      SelectTab(kTabStripElementId, 1), WaitForHide(kDialog1Id),
      Do([&]() { modal_widget2 = CreateAndShowTestDialog(kDialog2Id); }),
      WaitForShow(kDialog2Id), Do([&]() {
        modal_window2 = modal_widget2->GetNativeWindow().GetNativeNSWindow();
      }),
      // Focus the omnibox. The browser window becomes key, and the modal
      // remains visible but loses key status.
      MoveMouseTo(kOmniboxElementId), ClickMouse(),
      PollUntil([&]() -> bool { return [browser_window isKeyWindow]; },
                "Wait for browser window to be key"),
      // Click the web contents scrim on Tab 2. This should return focus to the
      // modal associated with the visible tab.
      MoveMouseTo(ContentsWebView::kContentsWebViewElementId), ClickMouse(),
      // Verify that Tab 2's modal becomes key, NOT Tab 1's modal (which is
      // hidden but still a child window).
      PollUntil([&]() -> bool { return [modal_window2 isKeyWindow]; },
                "Wait for modal 2 to become key"),
      Check([&]() { return ![modal_window1 isKeyWindow]; }));
}

}  // namespace
