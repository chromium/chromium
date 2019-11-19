// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_test_util.h"

#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

ExtensionsMenuTestUtil::ExtensionsMenuTestUtil(Browser* browser)
    : scoped_allow_extensions_menu_instances_(
          ExtensionsMenuView::AllowInstancesForTesting()),
      browser_(browser),
      extensions_container_(BrowserView::GetBrowserViewForBrowser(browser_)
                                ->toolbar()
                                ->extensions_container()),
      menu_view_(std::make_unique<ExtensionsMenuView>(
          extensions_container_->extensions_button(),
          browser_,
          extensions_container_)) {
  menu_view_->set_owned_by_client();
}
ExtensionsMenuTestUtil::~ExtensionsMenuTestUtil() = default;

int ExtensionsMenuTestUtil::NumberOfBrowserActions() {
  return menu_view_->extensions_menu_items_for_testing().size();
}

int ExtensionsMenuTestUtil::VisibleBrowserActions() {
  int visible_icons = 0;
  for (const auto& id_and_view : extensions_container_->icons_for_testing()) {
    if (id_and_view.second->GetVisible())
      ++visible_icons;
  }
  return visible_icons;
}

void ExtensionsMenuTestUtil::InspectPopup(int index) {
  // TODO(https://crbug.com/984654): Implement this.
  NOTREACHED();
}

bool ExtensionsMenuTestUtil::HasIcon(int index) {
  ExtensionsMenuItemView* view = GetMenuItemViewAtIndex(index);
  DCHECK(view);
  return !view->primary_action_button_for_testing()
              ->GetImage(views::Button::STATE_NORMAL)
              .isNull();
}

gfx::Image ExtensionsMenuTestUtil::GetIcon(int index) {
  ExtensionsMenuItemView* view = GetMenuItemViewAtIndex(index);
  DCHECK(view);
  return gfx::Image(view->primary_action_button_for_testing()->GetImage(
      views::Button::STATE_NORMAL));
}

void ExtensionsMenuTestUtil::Press(int index) {
  ExtensionsMenuItemView* view = GetMenuItemViewAtIndex(index);
  DCHECK(view);
  ExtensionsMenuButton* primary_button =
      view->primary_action_button_for_testing();

  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0);
  // ExtensionsMenuButton::ButtonPressed() is private; workaround by casting to
  // to a ButtonListener.
  static_cast<views::ButtonListener*>(primary_button)
      ->ButtonPressed(primary_button, event);
}

std::string ExtensionsMenuTestUtil::GetExtensionId(int index) {
  ExtensionsMenuItemView* view = GetMenuItemViewAtIndex(index);
  DCHECK(view);
  return view->view_controller_for_testing()->GetId();
}

std::string ExtensionsMenuTestUtil::GetTooltip(int index) {
  ExtensionsMenuItemView* view = GetMenuItemViewAtIndex(index);
  DCHECK(view);
  ExtensionsMenuButton* primary_button =
      view->primary_action_button_for_testing();
  return base::UTF16ToUTF8(primary_button->GetTooltipText(gfx::Point()));
}

gfx::NativeView ExtensionsMenuTestUtil::GetPopupNativeView() {
  ToolbarActionViewController* popup_owner =
      extensions_container_->popup_owner_for_testing();
  return popup_owner ? popup_owner->GetPopupNativeView() : nullptr;
}

bool ExtensionsMenuTestUtil::HasPopup() {
  return !!GetPopupNativeView();
}

gfx::Size ExtensionsMenuTestUtil::GetPopupSize() {
  gfx::NativeView popup = GetPopupNativeView();
  views::Widget* widget = views::Widget::GetWidgetForNativeView(popup);
  return widget->GetWindowBoundsInScreen().size();
}

bool ExtensionsMenuTestUtil::HidePopup() {
  // ExtensionsToolbarContainer::HideActivePopup() is private. Get around it by
  // casting to an ExtensionsContainer.
  static_cast<ExtensionsContainer*>(extensions_container_)->HideActivePopup();
  return !HasPopup();
}

bool ExtensionsMenuTestUtil::ActionButtonWantsToRun(size_t index) {
  // TODO(devlin): Investigate if wants-to-run behavior is still necessary.
  NOTREACHED();
  return false;
}

void ExtensionsMenuTestUtil::SetWidth(int width) {
  extensions_container_->SetSize(
      gfx::Size(width, extensions_container_->height()));
}

ToolbarActionsBar* ExtensionsMenuTestUtil::GetToolbarActionsBar() {
  // TODO(https://crbug.com/984654): There is no associated ToolbarActionsBar
  // with the ExtensionsMenu implementation. We should audit call sites, and
  // determine whether the functionality is specific to the old implementation,
  // or if callers should be updated to use the ExtensionsContainer interface.
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<BrowserActionTestUtil>
ExtensionsMenuTestUtil::CreateOverflowBar(Browser* browser) {
  // There is no overflow bar with the ExtensionsMenu implementation.
  NOTREACHED();
  return nullptr;
}

gfx::Size ExtensionsMenuTestUtil::GetMinPopupSize() {
  return gfx::Size(ExtensionPopup::kMinWidth, ExtensionPopup::kMinHeight);
}

gfx::Size ExtensionsMenuTestUtil::GetMaxPopupSize() {
  return gfx::Size(ExtensionPopup::kMaxWidth, ExtensionPopup::kMaxHeight);
}

bool ExtensionsMenuTestUtil::CanBeResized() {
  // TODO(https://crbug.com/984654): Implement this.
  NOTREACHED();
  return false;
}

ExtensionsMenuItemView* ExtensionsMenuTestUtil::GetMenuItemViewAtIndex(
    int index) {
  std::vector<ExtensionsMenuItemView*> menu_items =
      menu_view_->extensions_menu_items_for_testing();
  if (index >= base::checked_cast<int>(menu_items.size()))
    return nullptr;
  return menu_items[index];
}
