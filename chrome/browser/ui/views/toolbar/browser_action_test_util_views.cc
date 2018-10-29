// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/browser_action_test_util.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_action_test_util_views.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/views/test/test_views.h"
#include "ui/views/widget/widget.h"

namespace {

// The views-specific implementation of the TestToolbarActionsBarHelper, which
// creates and owns a BrowserActionsContainer.
class TestToolbarActionsBarHelperViews
    : public TestToolbarActionsBarHelper,
      public BrowserActionsContainer::Delegate {
 public:
  TestToolbarActionsBarHelperViews(Browser* browser,
                                   BrowserActionsContainer* main_bar)
      : browser_actions_container_(
            new BrowserActionsContainer(browser, main_bar, this, true)) {
    container_parent_.set_owned_by_client();
    container_parent_.SetSize(gfx::Size(1000, 1000));
    container_parent_.Layout();
    container_parent_.AddChildView(browser_actions_container_);
  }

  BrowserActionsContainer* browser_actions_container() {
    return browser_actions_container_;
  }

  // Overridden from BrowserActionsContainer::Delegate:
  views::MenuButton* GetOverflowReferenceView() override { return nullptr; }
  base::Optional<int> GetMaxBrowserActionsWidth() const override {
    return base::Optional<int>();
  }
  std::unique_ptr<ToolbarActionsBar> CreateToolbarActionsBar(
      ToolbarActionsBarDelegate* delegate,
      Browser* browser,
      ToolbarActionsBar* main_bar) const override {
    return std::make_unique<ToolbarActionsBar>(delegate, browser, main_bar);
  }

 private:
  // The created BrowserActionsContainer. Owned by |container_parent_|.
  BrowserActionsContainer* browser_actions_container_;

  // The parent of the BrowserActionsContainer, which directly owns the
  // container as part of the views hierarchy.
  views::ResizeAwareParentView container_parent_;

  DISALLOW_COPY_AND_ASSIGN(TestToolbarActionsBarHelperViews);
};

BrowserActionsContainer* GetContainer(Browser* browser,
                                      TestToolbarActionsBarHelper* helper) {
  if (helper) {
    return static_cast<TestToolbarActionsBarHelperViews*>(helper)
        ->browser_actions_container();
  }
  return BrowserView::GetBrowserViewForBrowser(browser)->toolbar()->
      browser_actions();
}

}  // namespace

BrowserActionTestUtilViews::BrowserActionTestUtilViews(Browser* browser)
    : BrowserActionTestUtilViews(browser, true) {}

BrowserActionTestUtilViews::BrowserActionTestUtilViews(Browser* browser,
                                                       bool is_real_window)
    : browser_(browser) {
  if (!is_real_window)
    test_helper_.reset(new TestToolbarActionsBarHelperViews(browser, nullptr));
}

BrowserActionTestUtilViews::~BrowserActionTestUtilViews() {}

int BrowserActionTestUtilViews::NumberOfBrowserActions() {
  return GetContainer(browser_, test_helper_.get())->num_toolbar_actions();
}

int BrowserActionTestUtilViews::VisibleBrowserActions() {
  return GetContainer(browser_, test_helper_.get())->VisibleBrowserActions();
}

void BrowserActionTestUtilViews::InspectPopup(int index) {
  ToolbarActionView* view =
      GetContainer(browser_, test_helper_.get())->GetToolbarActionViewAt(index);
  static_cast<ExtensionActionViewController*>(view->view_controller())->
      InspectPopup();
}

bool BrowserActionTestUtilViews::HasIcon(int index) {
  return !GetContainer(browser_, test_helper_.get())
              ->GetToolbarActionViewAt(index)
              ->GetImage(views::Button::STATE_NORMAL)
              .isNull();
}

gfx::Image BrowserActionTestUtilViews::GetIcon(int index) {
  gfx::ImageSkia icon = GetContainer(browser_, test_helper_.get())
                            ->GetToolbarActionViewAt(index)
                            ->GetIconForTest();
  return gfx::Image(icon);
}

void BrowserActionTestUtilViews::Press(int index) {
  GetContainer(browser_, test_helper_.get())
      ->GetToolbarActionViewAt(index)
      ->view_controller()
      ->ExecuteAction(true);
}

std::string BrowserActionTestUtilViews::GetExtensionId(int index) {
  return GetContainer(browser_, test_helper_.get())
      ->GetToolbarActionViewAt(index)
      ->view_controller()
      ->GetId();
}

std::string BrowserActionTestUtilViews::GetTooltip(int index) {
  base::string16 text;
  GetContainer(browser_, test_helper_.get())
      ->GetToolbarActionViewAt(index)
      ->GetTooltipText(gfx::Point(), &text);
  return base::UTF16ToUTF8(text);
}

gfx::NativeView BrowserActionTestUtilViews::GetPopupNativeView() {
  ToolbarActionViewController* popup_owner =
      GetToolbarActionsBar()->popup_owner();
  return popup_owner ? popup_owner->GetPopupNativeView() : nullptr;
}

bool BrowserActionTestUtilViews::HasPopup() {
  return GetPopupNativeView() != nullptr;
}

gfx::Size BrowserActionTestUtilViews::GetPopupSize() {
  gfx::NativeView popup = GetPopupNativeView();
  views::Widget* widget = views::Widget::GetWidgetForNativeView(popup);
  return widget->GetWindowBoundsInScreen().size();
}

bool BrowserActionTestUtilViews::HidePopup() {
  GetToolbarActionsBar()->HideActivePopup();
  return !HasPopup();
}

bool BrowserActionTestUtilViews::ActionButtonWantsToRun(size_t index) {
  return GetContainer(browser_, test_helper_.get())
      ->GetToolbarActionViewAt(index)
      ->wants_to_run_for_testing();
}

void BrowserActionTestUtilViews::SetWidth(int width) {
  BrowserActionsContainer* container =
      GetContainer(browser_, test_helper_.get());
  container->SetSize(gfx::Size(width, container->height()));
}

ToolbarActionsBar* BrowserActionTestUtilViews::GetToolbarActionsBar() {
  return GetContainer(browser_, test_helper_.get())->toolbar_actions_bar();
}

std::unique_ptr<BrowserActionTestUtil>
BrowserActionTestUtilViews::CreateOverflowBar() {
  CHECK(!GetToolbarActionsBar()->in_overflow_mode())
      << "Only a main bar can create an overflow bar!";
  return base::WrapUnique(new BrowserActionTestUtilViews(browser_, this));
}

gfx::Size BrowserActionTestUtilViews::GetMinPopupSize() {
  return gfx::Size(ExtensionPopup::kMinWidth, ExtensionPopup::kMinHeight);
}

gfx::Size BrowserActionTestUtilViews::GetMaxPopupSize() {
  return gfx::Size(ExtensionPopup::kMaxWidth, ExtensionPopup::kMaxHeight);
}

bool BrowserActionTestUtilViews::CanBeResized() {
  BrowserActionsContainer* container =
      BrowserView::GetBrowserViewForBrowser(browser_)
          ->toolbar()
          ->browser_actions();

  // The container can only be resized if we can start a drag for the view.
  DCHECK_LE(1u, container->num_toolbar_actions());
  ToolbarActionView* action_view = container->GetToolbarActionViewAt(0);
  gfx::Point point(action_view->x(), action_view->y());
  return container->CanStartDragForView(action_view, point, point);
}

BrowserActionTestUtilViews::BrowserActionTestUtilViews(
    Browser* browser,
    BrowserActionTestUtilViews* main_bar)
    : browser_(browser),
      test_helper_(new TestToolbarActionsBarHelperViews(
          browser_,
          GetContainer(browser_, main_bar->test_helper_.get()))) {}

// static
std::unique_ptr<BrowserActionTestUtil> BrowserActionTestUtil::Create(
    Browser* browser,
    bool is_real_window) {
  return std::make_unique<BrowserActionTestUtilViews>(browser, is_real_window);
}
