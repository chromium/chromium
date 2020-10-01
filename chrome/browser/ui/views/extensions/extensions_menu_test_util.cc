// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_test_util.h"

#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
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
#include "ui/views/test/button_test_api.h"
#include "ui/views/view.h"

// A view wrapper class that owns the ExtensionsToolbarContainer.
// This is used when we don't have a "real" browser window, because the
// TestBrowserWindow does not have a view instantiated for the container.
class ExtensionsMenuTestUtil::Wrapper {
 public:
  explicit Wrapper(Browser* browser)
      : extensions_container_(new ExtensionsToolbarContainer(browser)) {
    container_parent_.SetSize(gfx::Size(1000, 1000));
    container_parent_.Layout();
    container_parent_.AddChildView(extensions_container_);
  }
  ~Wrapper() = default;

  Wrapper(const Wrapper& other) = delete;
  Wrapper& operator=(const Wrapper& other) = delete;

  ExtensionsToolbarContainer* extensions_container() {
    return extensions_container_;
  }

 private:
  views::View container_parent_;
  ExtensionsToolbarContainer* extensions_container_ = nullptr;
};

ExtensionsMenuTestUtil::ExtensionsMenuTestUtil(Browser* browser,
                                               bool is_real_window)
    : scoped_allow_extensions_menu_instances_(
          ExtensionsMenuView::AllowInstancesForTesting()),
      browser_(browser) {
  if (is_real_window) {
    extensions_container_ = BrowserView::GetBrowserViewForBrowser(browser_)
                                ->toolbar()
                                ->extensions_container();
  } else {
    wrapper_ = std::make_unique<Wrapper>(browser);
    extensions_container_ = wrapper_->extensions_container();
  }
  menu_view_ = std::make_unique<ExtensionsMenuView>(
      extensions_container_->extensions_button(), browser_,
      extensions_container_, true);
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
  ExtensionsMenuItemView* view = GetMenuItemViewAtIndex(index);
  DCHECK(view);
  static_cast<ExtensionActionViewController*>(view->view_controller())
      ->InspectPopup();
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
  views::test::ButtonTestApi(primary_button).NotifyClick(event);
}

std::string ExtensionsMenuTestUtil::GetExtensionId(int index) {
  ExtensionsMenuItemView* view = GetMenuItemViewAtIndex(index);
  DCHECK(view);
  return view->view_controller()->GetId();
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

bool ExtensionsMenuTestUtil::HidePopup() {
  // ExtensionsToolbarContainer::HideActivePopup() is private. Get around it by
  // casting to an ExtensionsContainer.
  static_cast<ExtensionsContainer*>(extensions_container_)->HideActivePopup();
  return !HasPopup();
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

ExtensionsContainer* ExtensionsMenuTestUtil::GetExtensionsContainer() {
  return extensions_container_;
}

std::unique_ptr<ExtensionActionTestHelper>
ExtensionsMenuTestUtil::CreateOverflowBar(Browser* browser) {
  // There is no overflow bar with the ExtensionsMenu implementation.
  NOTREACHED();
  return nullptr;
}

void ExtensionsMenuTestUtil::LayoutForOverflowBar() {
  // There is no overflow bar with the ExtensionsMenu implementation.
  NOTREACHED();
}

gfx::Size ExtensionsMenuTestUtil::GetMinPopupSize() {
  return gfx::Size(ExtensionPopup::kMinWidth, ExtensionPopup::kMinHeight);
}

gfx::Size ExtensionsMenuTestUtil::GetMaxPopupSize() {
  return gfx::Size(ExtensionPopup::kMaxWidth, ExtensionPopup::kMaxHeight);
}

gfx::Size ExtensionsMenuTestUtil::GetToolbarActionSize() {
  return extensions_container_->GetToolbarActionSize();
}

ExtensionsMenuItemView* ExtensionsMenuTestUtil::GetMenuItemViewAtIndex(
    int index) {
  std::vector<ExtensionsMenuItemView*> menu_items =
      menu_view_->extensions_menu_items_for_testing();
  if (index >= base::checked_cast<int>(menu_items.size()))
    return nullptr;
  return menu_items[index];
}
