// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_test_util.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"

#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_coordinator.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "extensions/common/extension_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

ExtensionsMenuTestUtil::ExtensionsMenuTestUtil(Browser* browser)
    : scoped_allow_extensions_menu_instances_(
          ExtensionsMenuView::AllowInstancesForTesting()),
      browser_(browser) {
  extensions_container_ = BrowserView::GetBrowserViewForBrowser(browser_)
                              ->toolbar()
                              ->extensions_container();

  std::unique_ptr<views::BubbleDialogDelegate> bubble_dialog;
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    bubble_dialog =
        extensions_container_->GetExtensionsMenuCoordinatorForTesting()
            ->CreateExtensionsMenuBubbleDialogDelegateForTesting(
                extensions_container_->GetExtensionsButton(),
                extensions_container_);
  } else {
    bubble_dialog = std::make_unique<ExtensionsMenuView>(
        extensions_container_->GetExtensionsButton(), browser_,
        extensions_container_);
    menu_view_ = views::AsViewClass<ExtensionsMenuView>(
        bubble_dialog->GetContentsView());

    menu_view_->View::AddObserver(this);
  }

  views::BubbleDialogDelegate::CreateBubble(std::move(bubble_dialog));
}

ExtensionsMenuTestUtil::~ExtensionsMenuTestUtil() {
  if (!menu_view_) {
    return;
  }

  // Close the menu if when we own the menu view.
  menu_view_->GetWidget()->CloseNow();
}

void ExtensionsMenuTestUtil::OnViewIsDeleting(views::View* observed_view) {
  CHECK_EQ(observed_view, menu_view_);
  menu_view_ = nullptr;
}

int ExtensionsMenuTestUtil::NumberOfBrowserActions() {
  return extensions_container_->GetNumberOfActionsForTesting();
}

bool ExtensionsMenuTestUtil::HasAction(const extensions::ExtensionId& id) {
  return extensions_container_->GetActionForId(id) != nullptr;
}

void ExtensionsMenuTestUtil::InspectPopup(const extensions::ExtensionId& id) {
  auto* view_controller = static_cast<ExtensionActionViewController*>(
      extensions_container_->GetActionForId(id));
  DCHECK(view_controller);
  view_controller->InspectPopup();
}

gfx::Image ExtensionsMenuTestUtil::GetIcon(const extensions::ExtensionId& id) {
  ToolbarActionView* action_view = extensions_container_->GetViewForId(id);
  DCHECK(action_view);
  return gfx::Image(action_view->GetIconForTest());
}

void ExtensionsMenuTestUtil::Press(const extensions::ExtensionId& id) {
  ExtensionMenuItemView* view = GetMenuItemViewForId(id);
  DCHECK(view);
  ExtensionsMenuButton* primary_button =
      view->primary_action_button_for_testing();

  ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi(primary_button).NotifyClick(event);
}

gfx::NativeView ExtensionsMenuTestUtil::GetPopupNativeView() {
  ToolbarActionViewController* popup_owner =
      extensions_container_->popup_owner_for_testing();
  return popup_owner ? popup_owner->GetPopupNativeView() : nullptr;
}

bool ExtensionsMenuTestUtil::HasPopup() {
  return !!GetPopupNativeView();
}

bool ExtensionsMenuTestUtil::HidePopup() {
  // ExtensionsToolbarContainer::HideActivePopup() is private. Get around it by
  // casting to an ExtensionsContainer.
  static_cast<ExtensionsContainer*>(extensions_container_)->HideActivePopup();
  return !HasPopup();
}

void ExtensionsMenuTestUtil::WaitForExtensionsContainerLayout() {
  views::test::WaitForAnimatingLayoutManager(
      static_cast<views::View*>(extensions_container_));
}

gfx::Size ExtensionsMenuTestUtil::GetMinPopupSize() {
  return ExtensionPopup::kMinSize;
}

gfx::Size ExtensionsMenuTestUtil::GetMaxPopupSize() {
  return ExtensionPopup::kMaxSize;
}

gfx::Size ExtensionsMenuTestUtil::GetMaxAvailableSizeToFitBubbleOnScreen(
    const extensions::ExtensionId& id) {
#if BUILDFLAG(IS_OZONE)
  if (!ui::OzonePlatform::GetInstance()
           ->GetPlatformProperties()
           .supports_global_screen_coordinates) {
    return ExtensionPopup::kMaxSize;
  }
#endif
  auto* view_delegate = static_cast<ToolbarActionViewDelegateViews*>(
      static_cast<ExtensionActionViewController*>(
          extensions_container_->GetActionForId(id))
          ->view_delegate());
  return views::BubbleDialogDelegate::GetMaxAvailableScreenSpaceToPlaceBubble(
      view_delegate->GetReferenceButtonForPopup(),
      views::BubbleBorder::TOP_RIGHT,
      views::PlatformStyle::kAdjustBubbleIfOffscreen,
      views::BubbleFrameView::PreferredArrowAdjustment::kMirror);
}

ExtensionMenuItemView* ExtensionsMenuTestUtil::GetMenuItemViewForId(
    const extensions::ExtensionId& id) {
  base::flat_set<raw_ptr<ExtensionMenuItemView, CtnExperimental>> menu_items;
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    ExtensionsMenuMainPageView* main_page =
        extensions_container_->GetExtensionsMenuCoordinatorForTesting()
            ->GetControllerForTesting()
            ->GetMainPageViewForTesting();
    DCHECK(main_page);
    auto items = main_page->GetMenuItems();
    menu_items = {items.begin(), items.end()};

  } else {
    menu_items = menu_view_->extensions_menu_items_for_testing();
  }

  auto iter =
      base::ranges::find(menu_items, id, [](ExtensionMenuItemView* view) {
        return view->view_controller()->GetId();
      });
  return (iter == menu_items.end()) ? nullptr : *iter;
}

// static
std::unique_ptr<ExtensionActionTestHelper> ExtensionActionTestHelper::Create(
    Browser* browser) {
  return std::make_unique<ExtensionsMenuTestUtil>(browser);
}
