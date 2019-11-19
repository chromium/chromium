// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/browser_action_test_util.h"

#include <stddef.h>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_test_util.h"
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

// A helper class that owns an instance of a BrowserActionsContainer; this is
// used when testing without an associated browser window, or if this is for
// the overflow version of the bar.
class BrowserActionTestUtilViews::TestToolbarActionsBarHelper
    : public BrowserActionsContainer::Delegate {
 public:
  TestToolbarActionsBarHelper(Browser* browser,
                              BrowserActionsContainer* main_bar)
      : browser_actions_container_(
            new BrowserActionsContainer(browser, main_bar, this, true)) {
    container_parent_.set_owned_by_client();
    container_parent_.SetSize(gfx::Size(1000, 1000));
    container_parent_.Layout();
    container_parent_.AddChildView(browser_actions_container_);
  }
  TestToolbarActionsBarHelper(const TestToolbarActionsBarHelper&) = delete;
  TestToolbarActionsBarHelper& operator=(const TestToolbarActionsBarHelper&) =
      delete;
  ~TestToolbarActionsBarHelper() override = default;

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
  BrowserActionsContainer* const browser_actions_container_;

  // The parent of the BrowserActionsContainer, which directly owns the
  // container as part of the views hierarchy.
  views::ResizeAwareParentView container_parent_;
};

BrowserActionTestUtilViews::~BrowserActionTestUtilViews() = default;

int BrowserActionTestUtilViews::NumberOfBrowserActions() {
  return browser_actions_container_->num_toolbar_actions();
}

int BrowserActionTestUtilViews::VisibleBrowserActions() {
  return browser_actions_container_->VisibleBrowserActions();
}

void BrowserActionTestUtilViews::InspectPopup(int index) {
  ToolbarActionView* view =
      browser_actions_container_->GetToolbarActionViewAt(index);
  static_cast<ExtensionActionViewController*>(view->view_controller())->
      InspectPopup();
}

bool BrowserActionTestUtilViews::HasIcon(int index) {
  return !browser_actions_container_->GetToolbarActionViewAt(index)
              ->GetImage(views::Button::STATE_NORMAL)
              .isNull();
}

gfx::Image BrowserActionTestUtilViews::GetIcon(int index) {
  gfx::ImageSkia icon =
      browser_actions_container_->GetToolbarActionViewAt(index)
          ->GetIconForTest();
  return gfx::Image(icon);
}

void BrowserActionTestUtilViews::Press(int index) {
  browser_actions_container_->GetToolbarActionViewAt(index)
      ->view_controller()
      ->ExecuteAction(true);
}

std::string BrowserActionTestUtilViews::GetExtensionId(int index) {
  return browser_actions_container_->GetToolbarActionViewAt(index)
      ->view_controller()
      ->GetId();
}

std::string BrowserActionTestUtilViews::GetTooltip(int index) {
  base::string16 tooltip =
      browser_actions_container_->GetToolbarActionViewAt(index)->GetTooltipText(
          gfx::Point());
  return base::UTF16ToUTF8(tooltip);
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
  return browser_actions_container_->GetToolbarActionViewAt(index)
      ->wants_to_run_for_testing();
}

void BrowserActionTestUtilViews::SetWidth(int width) {
  browser_actions_container_->SetSize(
      gfx::Size(width, browser_actions_container_->height()));
}

ToolbarActionsBar* BrowserActionTestUtilViews::GetToolbarActionsBar() {
  return browser_actions_container_->toolbar_actions_bar();
}

std::unique_ptr<BrowserActionTestUtil>
BrowserActionTestUtilViews::CreateOverflowBar(Browser* browser) {
  CHECK(!GetToolbarActionsBar()->in_overflow_mode())
      << "Only a main bar can create an overflow bar!";

  return base::WrapUnique(new BrowserActionTestUtilViews(
      std::make_unique<TestToolbarActionsBarHelper>(
          browser, browser_actions_container_)));
}

gfx::Size BrowserActionTestUtilViews::GetMinPopupSize() {
  return gfx::Size(ExtensionPopup::kMinWidth, ExtensionPopup::kMinHeight);
}

gfx::Size BrowserActionTestUtilViews::GetMaxPopupSize() {
  return gfx::Size(ExtensionPopup::kMaxWidth, ExtensionPopup::kMaxHeight);
}

bool BrowserActionTestUtilViews::CanBeResized() {
  // The container can only be resized if we can start a drag for the view.
  DCHECK_LE(1u, browser_actions_container_->num_toolbar_actions());
  ToolbarActionView* action_view =
      browser_actions_container_->GetToolbarActionViewAt(0);
  gfx::Point point(action_view->x(), action_view->y());
  return browser_actions_container_->CanStartDragForView(action_view, point,
                                                         point);
}

BrowserActionTestUtilViews::BrowserActionTestUtilViews(
    BrowserActionsContainer* browser_actions_container)
    : browser_actions_container_(browser_actions_container) {}

BrowserActionTestUtilViews::BrowserActionTestUtilViews(
    std::unique_ptr<TestToolbarActionsBarHelper> test_helper)
    : test_helper_(std::move(test_helper)),
      browser_actions_container_(test_helper_->browser_actions_container()) {}

// static
std::unique_ptr<BrowserActionTestUtil> BrowserActionTestUtil::Create(
    Browser* browser,
    bool is_real_window) {
  // If the ExtensionsMenu is enabled, then use a separate implementation of
  // the BrowserActionTestUtil.
  if (base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu))
    return std::make_unique<ExtensionsMenuTestUtil>(browser);

  std::unique_ptr<BrowserActionTestUtil> browser_action_test_util;

  if (is_real_window) {
    browser_action_test_util = base::WrapUnique(new BrowserActionTestUtilViews(
        BrowserView::GetBrowserViewForBrowser(browser)
            ->toolbar()
            ->browser_actions()));
  } else {
    // This is the main bar.
    BrowserActionsContainer* main_bar = nullptr;
    browser_action_test_util = base::WrapUnique(new BrowserActionTestUtilViews(
        std::make_unique<
            BrowserActionTestUtilViews::TestToolbarActionsBarHelper>(
            browser, main_bar)));
  }

  return browser_action_test_util;
}
