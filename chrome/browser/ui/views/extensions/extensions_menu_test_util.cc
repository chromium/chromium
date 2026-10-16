// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_test_util.h"

#include <algorithm>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_action_view_model.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/extensions/extensions_container_views.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_delegate_desktop.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_entry_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_extensions_container.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

ExtensionsMenuTestUtil::ExtensionsMenuTestUtil(BrowserWindowInterface* browser)
    : scoped_allow_extensions_menu_instances_(
          ExtensionsMenuView::AllowInstancesForTesting()),
      browser_(browser) {
  // ToolbarView only creates an ExtensionsToolbarDesktop when the WebUI
  // extensions container is disabled; otherwise, the WebUI toolbar registers a
  // WebUIToolbarExtensionsContainer as the browser's ExtensionsContainer.
  extensions_toolbar_ = BrowserView::GetBrowserViewForBrowser(browser_)
                            ->toolbar()
                            ->extensions_container_for_testing();
  if (extensions_toolbar_) {
    extensions_container_ = extensions_toolbar_->GetToolbarViewModel();
    extensions_container_views_ = extensions_toolbar_;
  } else {
    CHECK(features::IsWebUIExtensionsContainerEnabled())
        << "No extensions container found. Is this a guest profile?";
    webui_extensions_container_ = static_cast<WebUIToolbarExtensionsContainer*>(
        ExtensionsContainer::From(*browser_));
    CHECK(webui_extensions_container_)
        << "The WebUI extensions container hasn't been created yet.";
    extensions_container_ = webui_extensions_container_;
    extensions_container_views_ = webui_extensions_container_;
  }
}

ExtensionsMenuTestUtil::~ExtensionsMenuTestUtil() {
  if (!IsExtensionsMenuShowing()) {
    return;
  }

  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    GetExtensionsMenuCoordinator()->Hide();
  } else {
    menu_view_->GetWidget()->CloseNow();
  }
}

void ExtensionsMenuTestUtil::OnViewIsDeleting(views::View* observed_view) {
  CHECK_EQ(observed_view, menu_view_);
  menu_view_ = nullptr;
}

int ExtensionsMenuTestUtil::NumberOfBrowserActions() {
  return extensions_toolbar_
             ? extensions_toolbar_->GetNumberOfActionsForTesting()
             : webui_extensions_container_->GetNumberOfActionsForTesting();
}

bool ExtensionsMenuTestUtil::HasAction(const extensions::ExtensionId& id) {
  return extensions_container_->GetActionForId(id) != nullptr;
}

void ExtensionsMenuTestUtil::InspectPopup(const extensions::ExtensionId& id) {
  auto* view_model = static_cast<ExtensionActionViewModel*>(
      extensions_container_->GetActionForId(id));
  DCHECK(view_model);
  view_model->InspectPopup();
}

gfx::Image ExtensionsMenuTestUtil::GetIcon(const extensions::ExtensionId& id) {
  if (extensions_toolbar_) {
    ToolbarActionView* action_view = extensions_toolbar_->GetViewForId(id);
    DCHECK(action_view);
    return gfx::Image(action_view->GetIconForTest());
  }
  return gfx::Image(webui_extensions_container_->GetActionIconForTesting(id));
}

void ExtensionsMenuTestUtil::Press(const extensions::ExtensionId& id) {
  OpenExtensionsMenu();

  HoverButton* action_button = GetActionButton(id);
  CHECK(action_button);

  views::test::ButtonTestApi(action_button).NotifyDefaultMouseClick();
}

gfx::NativeView ExtensionsMenuTestUtil::GetPopupNativeView() {
  ToolbarActionViewModel* popup_owner = GetPopupOwner();
  return popup_owner ? popup_owner->GetPopupNativeViewForTesting()
                     : gfx::NativeView();
}

bool ExtensionsMenuTestUtil::HasPopup() {
  return !!GetPopupNativeView();
}

bool ExtensionsMenuTestUtil::HidePopup() {
  extensions_container_->HideActivePopup();
  return !HasPopup();
}

void ExtensionsMenuTestUtil::WaitForExtensionsContainerLayout() {
  if (extensions_toolbar_) {
    views::test::WaitForAnimatingLayoutManager(
        static_cast<views::View*>(extensions_toolbar_));
    return;
  }

  // The WebUI toolbar registers a TrackedElement for each extension button
  // (and the extensions menu button) once the button has finished animating
  // in, and unregisters it when the button is removed from the DOM after
  // animating out. So the layout is done once there's an element for exactly
  // the actions that should be visible on the toolbar.
  const ToolbarActionsModel* model =
      ToolbarActionsModel::Get(browser_->GetProfile());
  EXPECT_TRUE(base::test::RunUntil([&]() {
    for (const ToolbarActionsModel::ActionId& action_id : model->action_ids()) {
      // The container may not have processed a newly added action yet.
      if (!webui_extensions_container_->GetActionForId(action_id)) {
        return false;
      }
      const bool has_anchor =
          webui_extensions_container_->GetExtensionAnchor(action_id) != nullptr;
      if (has_anchor !=
          webui_extensions_container_->IsActionVisibleOnToolbar(action_id)) {
        return false;
      }
    }
    // The extensions menu button is shown if and only if there are actions.
    const bool should_have_menu = !model->action_ids().empty();
    return (webui_extensions_container_->GetExtensionAnchor("") != nullptr) ==
           should_have_menu;
  })) << "Timed out waiting for the WebUI extensions container to lay out.";
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
  return views::BubbleDialogDelegate::GetMaxAvailableScreenSpaceToPlaceBubble(
      extensions_container_views_->GetReferenceButtonForPopup(id),
      views::BubbleBorder::TOP_RIGHT,
      views::PlatformStyle::kAdjustBubbleIfOffscreen,
      views::BubbleFrameView::PreferredArrowAdjustment::kMirror);
}

void ExtensionsMenuTestUtil::OpenExtensionsMenu() {
  if (IsExtensionsMenuShowing()) {
    return;
  }

  views::BubbleAnchor anchor =
      extensions_container_views_->GetExtensionsButtonAnchor();
  std::unique_ptr<views::BubbleDialogDelegate> bubble_dialog;
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    bubble_dialog = GetExtensionsMenuCoordinator()
                        ->CreateExtensionsMenuBubbleDialogDelegateForTesting(
                            anchor, extensions_container_views_);
  } else {
    bubble_dialog = std::make_unique<ExtensionsMenuView>(
        anchor, browser_, extensions_container_, extensions_container_views_);
    menu_view_ = views::AsViewClass<ExtensionsMenuView>(
        bubble_dialog->GetContentsView());

    menu_view_->View::AddObserver(this);
  }

  views::BubbleDialogDelegate::CreateBubbleDeprecated(
      std::move(bubble_dialog),
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
}

bool ExtensionsMenuTestUtil::IsExtensionsMenuShowing() {
  return base::FeatureList::IsEnabled(
             extensions_features::kExtensionsMenuAccessControl)
             ? GetExtensionsMenuCoordinator()->IsShowing()
             : menu_view_;
}

HoverButton* ExtensionsMenuTestUtil::GetActionButton(
    const extensions::ExtensionId& id) {
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    ExtensionsMenuMainPageView* main_page = GetExtensionsMenuCoordinator()
                                                ->GetDelegateForTesting()
                                                ->GetMainPageViewForTesting();
    DCHECK(main_page);
    std::vector<ExtensionsMenuEntryView*> menu_entries =
        main_page->GetMenuEntries();

    auto iter = std::find_if(menu_entries.begin(), menu_entries.end(),
                             [&id](ExtensionsMenuEntryView* entry) {
                               return entry->extension_id() == id;
                             });

    return (iter == menu_entries.end()) ? nullptr
                                        : (*iter)->action_button_for_testing();
  }

  base::flat_set<raw_ptr<ExtensionMenuItemView, CtnExperimental>> menu_items =
      menu_view_->extensions_menu_items_for_testing();
  auto iter = std::find_if(menu_items.begin(), menu_items.end(),
                           [&id](ExtensionMenuItemView* item) {
                             return item->view_model()->GetId() == id;
                           });

  return (iter == menu_items.end())
             ? nullptr
             : (*iter)->primary_action_button_for_testing();
}

ExtensionsMenuCoordinator*
ExtensionsMenuTestUtil::GetExtensionsMenuCoordinator() {
  return extensions_toolbar_
             ? extensions_toolbar_->GetExtensionsMenuCoordinatorForTesting()
             : webui_extensions_container_
                   ->GetExtensionsMenuCoordinatorForTesting();
}

ToolbarActionViewModel* ExtensionsMenuTestUtil::GetPopupOwner() {
  return extensions_toolbar_
             ? extensions_toolbar_->popup_owner_for_testing()
             : webui_extensions_container_->popup_owner_for_testing();
}

// static
std::unique_ptr<ExtensionActionTestHelper> ExtensionActionTestHelper::Create(
    BrowserWindowInterface* browser) {
  return std::make_unique<ExtensionsMenuTestUtil>(browser);
}
