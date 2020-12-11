// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_action_test_helper.h"

#include <stddef.h>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extension_action_test_helper_views.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_test_util.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/test_views.h"

// A helper class that owns an instance of a BrowserActionsContainer; this is
// used when testing without an associated browser window, or if this is for
// the overflow version of the bar.
class ExtensionActionTestHelperViews::TestToolbarActionsBarHelper
    : public BrowserActionsContainer::Delegate {
 public:
  TestToolbarActionsBarHelper(Browser* browser,
                              BrowserActionsContainer* main_bar)
      : browser_actions_container_(
            new BrowserActionsContainer(browser, main_bar, this, true)) {
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

ExtensionActionTestHelperViews::~ExtensionActionTestHelperViews() = default;

int ExtensionActionTestHelperViews::NumberOfBrowserActions() {
  return browser_actions_container_->num_toolbar_actions();
}

int ExtensionActionTestHelperViews::VisibleBrowserActions() {
  return browser_actions_container_->VisibleBrowserActions();
}

void ExtensionActionTestHelperViews::InspectPopup(int index) {
  ToolbarActionView* view =
      browser_actions_container_->GetToolbarActionViewAt(index);
  static_cast<ExtensionActionViewController*>(view->view_controller())
      ->InspectPopup();
}

bool ExtensionActionTestHelperViews::HasIcon(int index) {
  return !browser_actions_container_->GetToolbarActionViewAt(index)
              ->GetImage(views::Button::STATE_NORMAL)
              .isNull();
}

gfx::Image ExtensionActionTestHelperViews::GetIcon(int index) {
  gfx::ImageSkia icon =
      browser_actions_container_->GetToolbarActionViewAt(index)
          ->GetIconForTest();
  return gfx::Image(icon);
}

void ExtensionActionTestHelperViews::Press(int index) {
  browser_actions_container_->GetToolbarActionViewAt(index)
      ->view_controller()
      ->ExecuteAction(
          true, ToolbarActionViewController::InvocationSource::kToolbarButton);
}

std::string ExtensionActionTestHelperViews::GetExtensionId(int index) {
  return browser_actions_container_->GetToolbarActionViewAt(index)
      ->view_controller()
      ->GetId();
}

std::string ExtensionActionTestHelperViews::GetTooltip(int index) {
  base::string16 tooltip =
      browser_actions_container_->GetToolbarActionViewAt(index)->GetTooltipText(
          gfx::Point());
  return base::UTF16ToUTF8(tooltip);
}

gfx::NativeView ExtensionActionTestHelperViews::GetPopupNativeView() {
  ToolbarActionViewController* popup_owner =
      GetToolbarActionsBar()->popup_owner();
  return popup_owner ? popup_owner->GetPopupNativeView() : nullptr;
}

bool ExtensionActionTestHelperViews::HasPopup() {
  return GetPopupNativeView() != nullptr;
}

bool ExtensionActionTestHelperViews::HidePopup() {
  GetToolbarActionsBar()->HideActivePopup();
  return !HasPopup();
}

void ExtensionActionTestHelperViews::SetWidth(int width) {
  browser_actions_container_->SetSize(
      gfx::Size(width, browser_actions_container_->height()));
}

ToolbarActionsBar* ExtensionActionTestHelperViews::GetToolbarActionsBar() {
  return browser_actions_container_->toolbar_actions_bar();
}

ExtensionsContainer* ExtensionActionTestHelperViews::GetExtensionsContainer() {
  return GetToolbarActionsBar();
}

void ExtensionActionTestHelperViews::WaitForExtensionsContainerLayout() {
  // In case the feature |kExtensionsToolbarMenu| is disabled, the tests wait
  // for layout completion themselves instead of calling into the
  // |ExtensionActionTestHelper|.
  NOTREACHED();
}

std::unique_ptr<ExtensionActionTestHelper>
ExtensionActionTestHelperViews::CreateOverflowBar(Browser* browser) {
  CHECK(!GetToolbarActionsBar()->in_overflow_mode())
      << "Only a main bar can create an overflow bar!";

  return base::WrapUnique(new ExtensionActionTestHelperViews(
      std::make_unique<TestToolbarActionsBarHelper>(
          browser, browser_actions_container_)));
}

void ExtensionActionTestHelperViews::LayoutForOverflowBar() {
  CHECK(GetToolbarActionsBar()->in_overflow_mode());
  // This will update the container's preferred size which will in turn trigger
  // a layout in the parent ResizeAwareParentView.
  test_helper_->browser_actions_container()->RefreshToolbarActionViews();
}

gfx::Size ExtensionActionTestHelperViews::GetMinPopupSize() {
  return ExtensionPopup::kMinSize;
}

gfx::Size ExtensionActionTestHelperViews::GetMaxPopupSize() {
  return ExtensionPopup::kMaxSize;
}

gfx::Size ExtensionActionTestHelperViews::GetToolbarActionSize() {
  return GetToolbarActionsBar()->GetViewSize();
}

gfx::Size
ExtensionActionTestHelperViews::GetMaxAvailableSizeToFitBubbleOnScreen(
    int action_index) {
  return views::BubbleDialogDelegate::GetMaxAvailableScreenSpaceToPlaceBubble(
      browser_actions_container_->GetToolbarActionViewAt(action_index),
      views::BubbleBorder::TOP_RIGHT,
      views::PlatformStyle::kAdjustBubbleIfOffscreen,
      views::BubbleFrameView::PreferredArrowAdjustment::kMirror);
}

ExtensionActionTestHelperViews::ExtensionActionTestHelperViews(
    BrowserActionsContainer* browser_actions_container)
    : browser_actions_container_(browser_actions_container) {}

ExtensionActionTestHelperViews::ExtensionActionTestHelperViews(
    std::unique_ptr<TestToolbarActionsBarHelper> test_helper)
    : test_helper_(std::move(test_helper)),
      browser_actions_container_(test_helper_->browser_actions_container()) {}

// static
std::unique_ptr<ExtensionActionTestHelper> ExtensionActionTestHelper::Create(
    Browser* browser,
    bool is_real_window) {
  // If the ExtensionsMenu is enabled, then use a separate implementation of
  // the ExtensionActionTestHelper.
  if (base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu))
    return std::make_unique<ExtensionsMenuTestUtil>(browser, is_real_window);

  std::unique_ptr<ExtensionActionTestHelper> browser_action_test_util;

  if (is_real_window) {
    browser_action_test_util =
        base::WrapUnique(new ExtensionActionTestHelperViews(
            BrowserView::GetBrowserViewForBrowser(browser)
                ->toolbar()
                ->browser_actions()));
  } else {
    // This is the main bar.
    BrowserActionsContainer* main_bar = nullptr;
    browser_action_test_util =
        base::WrapUnique(new ExtensionActionTestHelperViews(
            std::make_unique<
                ExtensionActionTestHelperViews::TestToolbarActionsBarHelper>(
                browser, main_bar)));
  }

  return browser_action_test_util;
}
