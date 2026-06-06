// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_context_menu.h"

#include <algorithm>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/buildflags.h"
#include "components/favicon_base/favicon_types.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/view_shadow.h"

namespace {
// New main menu width.
constexpr int kMainMenuWidthWithSubmenuEnabled = 240;
// Default menu width for all menus, unless specified.
constexpr int kDefaultMenuWidth = 320;
// Maximum height of tab sub menu before scroll is activated.
constexpr int kMaxTabSubMenuHeight = 344;
// The shadow variation to use for `ViewShadow`
constexpr int kShadowOption = 3;
}  // namespace

OmniboxContextMenu::OmniboxContextMenu(views::Widget* parent_widget,
                                       OmniboxPopupFileSelector* file_selector,
                                       content::WebContents* web_contents,
                                       base::RepeatingClosure on_menu_closed)
    : parent_widget_(parent_widget),
      controller_(std::make_unique<OmniboxContextMenuController>(file_selector,
                                                                 web_contents)),
      on_menu_closed_(std::move(on_menu_closed)),
      web_contents_(web_contents->GetWeakPtr()) {
  std::unique_ptr<views::MenuItemView> menu =
      std::make_unique<views::MenuItemView>(this);
  menu_ = menu.get();
  menu_runner_ = std::make_unique<views::MenuRunner>(
      std::move(menu), views::MenuRunner::HAS_MNEMONICS |
                           views::MenuRunner::MENU_ITEM_CONTEXT_MENU);
  ui::SimpleMenuModel* menu_model = controller_->menu_model();
  menu_model->SetMenuModelDelegate(this);
  // Register `this` to listen for live icon updates on the secondary submenu.
  if (controller_->shared_tabs_menu_model()) {
    controller_->shared_tabs_menu_model()->SetMenuModelDelegate(this);
  }

  for (size_t i = 0; i < menu_model->GetItemCount(); ++i) {
    views::MenuItemView* item =
        views::MenuModelAdapter::AppendMenuItemFromModel(
            menu_model, i, menu_, menu_model->GetCommandIdAt(i));
    if (item) {
      // Add margins between menu items if they exist:
      // With icon size 16px per command ID/row, this results
      // in rows that are 34px tall.
      item->set_vertical_margin(9);
    }
    // If the top-level item is a real submenu container, recursively append its
    // underlying child items (tabs) to ensure the menu tree is fully populated.
    if (item && menu_model->GetTypeAt(i) == ui::MenuModel::TYPE_SUBMENU) {
      ui::MenuModel* submodel = menu_model->GetSubmenuModelAt(i);
      CHECK(submodel);
      for (size_t j = 0; j < submodel->GetItemCount(); ++j) {
        // Add margins between submenu items:
        // With icon size 16px per command ID/row, this results
        // in rows that are 34px tall.
        views::MenuItemView* subitem =
            views::MenuModelAdapter::AppendMenuItemFromModel(
                submodel, j, item, submodel->GetCommandIdAt(j));
        if (subitem) {
          subitem->set_vertical_margin(9);
        }
      }
    }
  }
}

int OmniboxContextMenu::GetMaxWidthForMenu(views::MenuItemView* menu) {
  if (!base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox) ||
      !base::FeatureList::IsEnabled(omnibox::kContextManagementInOmnibox)) {
    return kDefaultMenuWidth;
  }
  // If is top level menu, return main menu's width;
  // otherwise it is the submenu, so return submenu's (default width).
  int width =
      (menu == menu_) ? kMainMenuWidthWithSubmenuEnabled : kDefaultMenuWidth;
  // The context menu has drop shadow and borders drawn. Ensure that
  // those are not taken into account when calculating the minimum width.
  if (menu->HasSubmenu() && menu->GetSubmenu()->GetScrollViewContainer()) {
    width += menu->GetSubmenu()
                 ->GetScrollViewContainer()
                 ->outside_border_insets()
                 .width();
  }
  return width;
}
void OmniboxContextMenu::WillShowMenu(views::MenuItemView* menu) {
  // For both tabs and regular context menu:
  if (menu == menu_ ||
      menu->GetCommand() == IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU) {
    auto* scroll_container = menu->GetSubmenu()->GetScrollViewContainer();
    if (scroll_container) {
      if (menu->GetCommand() == IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU) {
        // Set up scroll capabilities for tabs submenu right before it is
        // rendered. Scroll is set up to start when max height is exceeded.
        gfx::Size pref_size = scroll_container->GetPreferredSize({});
        pref_size.set_height(
            std::min(pref_size.height(), kMaxTabSubMenuHeight));
        scroll_container->SetPreferredSize(pref_size);
      }

      // Add elevation shadow based on variation option `kShadowOption`
      // for both tabs submenu and context menu.
      if (!view_shadows_.contains(scroll_container)) {
        auto shadow = std::make_unique<views::ViewShadow>(scroll_container,
                                                          kShadowOption);
        int corner_radius = views::MenuConfig::instance().CornerRadiusForMenu(
            menu->GetMenuController());
        shadow->SetRoundedCornerRadius(corner_radius);
        view_shadows_[scroll_container] = std::move(shadow);
      }
    }
  }
}

OmniboxContextMenu::~OmniboxContextMenu() {
  if (controller_ && controller_->menu_model()) {
    controller_->menu_model()->SetMenuModelDelegate(nullptr);
    if (controller_->shared_tabs_menu_model()) {
      controller_->shared_tabs_menu_model()->SetMenuModelDelegate(nullptr);
    }
  }
}

void OmniboxContextMenu::RunMenuAt(const gfx::Point& point,
                                   ui::mojom::MenuSourceType source_type) {
  if (menu_ && menu_->HasSubmenu()) {
    if (base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox) &&
        base::FeatureList::IsEnabled(omnibox::kContextManagementInOmnibox)) {
      // Set main menu to the narrower width when a submenu is enabled.
      menu_->GetSubmenu()->set_minimum_preferred_width(
          kMainMenuWidthWithSubmenuEnabled);
      // Apply preferred width to each submenu width; this is more robust
      // than applying the width to the submenu itself or a command ID, which
      // causes the width of submenu items to be incorrect.
      for (views::MenuItemView* item : menu_->GetSubmenu()->GetMenuItems()) {
        if (item->HasSubmenu()) {
          item->GetSubmenu()->set_minimum_preferred_width(kDefaultMenuWidth);
        }
      }
    } else {
      menu_->GetSubmenu()->set_minimum_preferred_width(kDefaultMenuWidth);
    }
  }

  menu_runner_->RunMenuAt(parent_widget_, nullptr,
                          gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft, source_type);

  if (!web_contents_) {
    return;
  }
  // Hide the GLIC nudge when the side panel is opened.
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_.get());
  if (!browser_window_interface) {
    return;
  }
  auto* glic_nudge_controller =
      browser_window_interface->GetFeatures().glic_nudge_controller();
  if (!glic_nudge_controller) {
    return;
  }
  auto* active_tab_interface =
      browser_window_interface->GetActiveTabInterface();
  if (!active_tab_interface) {
    return;
  }

  glic_nudge_controller->UpdateNudgeLabel(
      browser_window_interface->GetActiveTabInterface()->GetContents(), "",
      std::nullopt, /*anchored_message_text=*/std::string(),
      glic::GlicNudgeActivity::kNudgeIgnoredOmniboxContextMenuInteraction,
      base::DoNothing());
}

void OmniboxContextMenu::ExecuteCommand(int command_id, int event_flags) {
  controller_->ExecuteCommand(command_id, event_flags);
}

// Needed so that titles display correctly.
const gfx::FontList* OmniboxContextMenu::GetLabelFontList(
    int command_id) const {
  ui::MenuModel* model = controller_->menu_model();
  size_t index = 0;
  ui::MenuModel::GetModelAndIndexForCommandId(command_id, &model, &index);
  return model->GetLabelFontListAt(index);
}

std::optional<SkColor> OmniboxContextMenu::GetLabelColor(int command_id) const {
  // Use STYLE_PRIMARY for title item. This aligns with 3-dot menu title style.
  return command_id == ui::MenuModel::kTitleId
             ? std::make_optional(
                   menu_->GetSubmenu()->GetColorProvider()->GetColor(
                       views::TypographyProvider::Get().GetColorId(
                           views::style::CONTEXT_MENU,
                           views::style::STYLE_PRIMARY)))
             : std::nullopt;
}

bool OmniboxContextMenu::IsCommandEnabled(int command_id) const {
  return controller_->IsCommandIdEnabled(command_id);
}

bool OmniboxContextMenu::IsCommandVisible(int command_id) const {
  return controller_->IsCommandIdVisible(command_id);
}

void OmniboxContextMenu::OnMenuClosed(views::MenuItemView* menu) {
  view_shadows_.clear();
  if (on_menu_closed_) {
    on_menu_closed_.Run();
  }
}

void OmniboxContextMenu::OnIconChanged(int command_id) {
  ui::SimpleMenuModel* model = controller_->menu_model();
  std::optional<size_t> index = model->GetIndexOfCommandId(command_id);
  // Update either 'tab section' or 'tab submenu':
  if (!index && controller_->shared_tabs_menu_model()) {
    model = controller_->shared_tabs_menu_model();
    index = model->GetIndexOfCommandId(command_id);
  }
  DCHECK(index.has_value());
  // Use `command_id` to find the item since the array indices have duplicates
  // due to container-relative indexing and do not map directly across submenus.
  views::MenuItemView* menu_item = menu_->GetMenuItemByID(command_id);
  if (menu_item) {
    menu_item->SetIcon(model->GetIconAt(index.value()));
  }
}
