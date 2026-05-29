// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDialogId);

// A helper to observe widget activation changes and ensure they don't happen
// unexpectedly.
class WidgetActivationObserver : public views::WidgetObserver {
 public:
  explicit WidgetActivationObserver(views::Widget* widget) : widget_(widget) {
    widget_->AddObserver(this);
  }
  ~WidgetActivationObserver() override {
    if (widget_) {
      widget_->RemoveObserver(this);
    }
  }

  void OnWidgetActivationChanged(views::Widget* widget, bool active) override {
    if (!active) {
      deactivated_count_++;
    }
  }

  void OnWidgetDestroying(views::Widget* widget) override {
    widget_->RemoveObserver(this);
    widget_ = nullptr;
  }

  int deactivated_count() const { return deactivated_count_; }

 private:
  raw_ptr<views::Widget> widget_;
  int deactivated_count_ = 0;
};

class TabModalFocusInteractiveUITestMac : public InteractiveBrowserTest {
 public:
  TabModalFocusInteractiveUITestMac() = default;
  ~TabModalFocusInteractiveUITestMac() override = default;

 protected:
  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    browser_view->GetWidget()->Activate();
  }

  std::unique_ptr<views::Widget> CreateAndShowTestDialog() {
    ui::DialogModel::Builder dialog_builder;
    dialog_builder.SetInternalName("TestDialog");
    dialog_builder.AddOkButton(
        base::DoNothing(), ui::DialogModel::Button::Params().SetId(kDialogId));

    tabs::TabInterface* tab_interface = browser()->GetActiveTabInterface();
    tabs::TabDialogManager* manager =
        tab_interface->GetTabFeatures()->tab_dialog_manager();

    auto model_host = views::BubbleDialogModelHost::CreateModal(
        dialog_builder.Build(), ui::mojom::ModalType::kChild);
    model_host->SetOwnershipOfNewWidget(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET);

    return manager->CreateAndShowDialog(
        model_host.release(),
        std::make_unique<tabs::TabDialogManager::Params>());
  }

  bool IsKeyWindow(views::Widget* widget) {
    if (!widget || widget->IsClosed()) {
      return false;
    }
    NSWindow* window = widget->GetNativeWindow().GetNativeNSWindow();
    return window && [window isKeyWindow];
  }
};

// Tests that clicking on the scrim does not steal focus from a tab-modal
// dialog.
IN_PROC_BROWSER_TEST_F(TabModalFocusInteractiveUITestMac,
                       ClickOnScrimMaintainsKeyWindow) {
  std::unique_ptr<views::Widget> modal_widget;
  std::unique_ptr<WidgetActivationObserver> observer;

  RunTestSequence(
      Do([&]() { modal_widget = CreateAndShowTestDialog(); }),
      WaitForShow(kDialogId),
      // Ensure the modal has become the key window.
      PollUntil([&]() { return IsKeyWindow(modal_widget.get()); },
                "Verify modal is key"),
      Do([&]() {
        observer =
            std::make_unique<WidgetActivationObserver>(modal_widget.get());
      }),
      // Click the web contents scrim.
      MoveMouseTo(ContentsWebView::kContentsWebViewElementId), ClickMouse(),
      // The modal should remain key window and never have deactivated.
      PollUntil([&]() { return IsKeyWindow(modal_widget.get()); },
                "Verify modal is still key"),
      Check([&]() { return observer->deactivated_count() == 0; },
            "Verify widget never deactivated"));
}

// Tests that clicking on the omnibox correctly makes the browser window key,
// even while a tab-modal dialog is active.
IN_PROC_BROWSER_TEST_F(TabModalFocusInteractiveUITestMac,
                       ClickOnOmniboxStealsKeyWindow) {
  std::unique_ptr<views::Widget> modal_widget;
  static constexpr char kModalRootViewName[] = "ModalRootView";
  RunTestSequence(
      Do([&]() { modal_widget = CreateAndShowTestDialog(); }),
      WaitForShow(kDialogId),
      PollUntil([&]() { return IsKeyWindow(modal_widget.get()); },
                "Verify modal is key"),
      // Click the omnibox.
      MoveMouseTo(kOmniboxElementId), ClickMouse(),
      // The modal should no longer be the key window.
      PollUntil([&]() { return !IsKeyWindow(modal_widget.get()); },
                "Verify modal is no longer key"),
      PollUntil(
          [&]() {
            return IsKeyWindow(
                BrowserView::GetBrowserViewForBrowser(browser())->GetWidget());
          },
          "Verify browser window is key"),
      // Click back on the dialog.
      NameView(kModalRootViewName,
               base::BindLambdaForTesting([&]() -> views::View* {
                 return modal_widget->GetRootView();
               })),
      MoveMouseTo(kModalRootViewName), ClickMouse(),
      // The modal should be key window again.
      PollUntil([&]() { return IsKeyWindow(modal_widget.get()); },
                "Verify modal is key again"));
}

// Tests that clicking on a toolbar button correctly makes the browser window
// key, and clicking back on the dialog makes the dialog key again.
IN_PROC_BROWSER_TEST_F(TabModalFocusInteractiveUITestMac,
                       ClickOnToolbarStealsKeyWindow) {
  std::unique_ptr<views::Widget> modal_widget;
  static constexpr char kModalRootViewName[] = "ModalRootView";
  RunTestSequence(
      Do([&]() { modal_widget = CreateAndShowTestDialog(); }),
      WaitForShow(kDialogId),
      PollUntil([&]() { return IsKeyWindow(modal_widget.get()); },
                "Verify modal is key"),
      // Click the Back button.
      MoveMouseTo(kToolbarBackButtonElementId), ClickMouse(),
      // The modal should no longer be the key window.
      PollUntil([&]() { return !IsKeyWindow(modal_widget.get()); },
                "Verify modal is no longer key"),
      PollUntil(
          [&]() {
            return IsKeyWindow(
                BrowserView::GetBrowserViewForBrowser(browser())->GetWidget());
          },
          "Verify browser window is key"),
      // Click back on the dialog.
      NameView(kModalRootViewName,
               base::BindLambdaForTesting([&]() -> views::View* {
                 return modal_widget->GetRootView();
               })),
      MoveMouseTo(kModalRootViewName), ClickMouse(),
      // The modal should be key window again.
      PollUntil([&]() { return IsKeyWindow(modal_widget.get()); },
                "Verify modal is key again"));
}

}  // namespace
