// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/test_support/app_menu_test_accessor.h"

#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/test/run_until.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/app_menu_control.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {

content::WebContents* GetWebContentsFromView(views::View* view) {
  if (!view) {
    return nullptr;
  }
  if (auto* web_view = views::AsViewClass<views::WebView>(view)) {
    return web_view->GetWebContents();
  }
  for (views::View* child : view->children()) {
    if (content::WebContents* contents = GetWebContentsFromView(child)) {
      return contents;
    }
  }
  return nullptr;
}

}  // namespace

AppMenuTestAccessor::AppMenuTestAccessor(BrowserWindowInterface* browser) {
  CHECK(browser);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  CHECK(browser_view);
  control_ = browser_view->toolbar_button_provider()->GetAppMenuControl();
  CHECK(control_);
}

AppMenuTestAccessor::AppMenuTestAccessor(AppMenuControl* control)
    : control_(control) {
  CHECK(control_);
}

AppMenuTestAccessor::~AppMenuTestAccessor() = default;

void AppMenuTestAccessor::ShowMenu(int run_types) {
  CHECK(control_);
  control_->ShowMenuWithFlags(run_types);
}

void AppMenuTestAccessor::CloseMenu() {
  CHECK(control_);
  control_->CloseMenu();
}

bool AppMenuTestAccessor::IsMenuShowing() const {
  CHECK(control_);
  return control_->IsMenuShowing();
}

AppMenu* AppMenuTestAccessor::GetAppMenu() const {
  CHECK(control_);
  return control_->GetAppMenu();
}

AppMenuModel* AppMenuTestAccessor::GetAppMenuModel() const {
  CHECK(control_);
  return control_->GetAppMenuModel();
}

views::MenuItemView* AppMenuTestAccessor::GetRootMenuItemView() const {
  AppMenu* menu = GetAppMenu();
  return menu ? menu->root_menu_item() : nullptr;
}

void AppMenuTestAccessor::ExecuteCommand(int command_id,
                                         int mouse_event_flags) {
  AppMenu* menu = GetAppMenu();
  CHECK(menu);
  menu->ExecuteCommand(command_id, mouse_event_flags);
}

bool AppMenuTestAccessor::IsElementIdAlerted(
    ui::ElementIdentifier element_id) const {
  AppMenuModel* model = GetAppMenuModel();
  CHECK(model);
  return model->IsElementIdAlerted(element_id);
}

void AppMenuTestAccessor::SetMenuTimerForTesting(base::ElapsedTimer timer) {
  AppMenu* menu = GetAppMenu();
  CHECK(menu);
  menu->SetTimerForTesting(std::move(timer));
}

bool AppMenuTestAccessor::HandleAccessibleAction(
    const ui::AXActionData& action_data) {
  CHECK(control_);
  if (views::View* view = control_->GetFocusablePaneView()) {
    if (view->HandleAccessibleAction(action_data)) {
      return true;
    }
  }

  // If the control is backed by WebUI, dispatch the accessibility action to
  // the WebUI app menu button in the WebContents.
  content::WebContents* web_contents =
      GetWebContentsFromView(control_->GetFocusablePaneView());
  if (!web_contents) {
    return false;
  }

  std::string acc_name = l10n_util::GetStringUTF8(IDS_ACCNAME_APP);

  content::WaitForAccessibilityTreeToContainNodeWithName(web_contents,
                                                         acc_name);

  content::FindAccessibilityNodeCriteria find_criteria;
  find_criteria.name = acc_name;
  ui::AXPlatformNodeDelegate* node =
      content::FindAccessibilityNode(web_contents, find_criteria);
  if (!node) {
    return false;
  }

  ui::AXActionData action_data_copy = action_data;
  action_data_copy.target_node_id = node->GetData().id;
  if (!node->AccessibilityPerformAction(action_data_copy)) {
    return false;
  }

  switch (action_data.action) {
    case ax::mojom::Action::kExpand:
      return base::test::RunUntil([&]() { return IsMenuShowing(); });
    case ax::mojom::Action::kCollapse:
      return base::test::RunUntil([&]() { return !IsMenuShowing(); });
    case ax::mojom::Action::kDoDefault: {
      const bool was_showing = IsMenuShowing();
      return base::test::RunUntil(
          [&]() { return IsMenuShowing() != was_showing; });
    }
    default:
      // TODO(crbug.com/562595290): Support additional accessibility actions as
      // needed.
      NOTREACHED();
  }
}

views::BubbleAnchor AppMenuTestAccessor::GetAnchor() const {
  CHECK(control_);
  return control_->GetAnchor();
}
