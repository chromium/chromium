// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/debug/profiler.h"
#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/views/app_menu/app_menu_section_action_item.h"
#include "chrome/browser/ui/views/app_menu/bookmarks_dynamic_menu.h"
#include "chrome/browser/ui/views/app_menu/recent_tabs_dynamic_menu.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/menus/simple_menu_model.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ActionAppMenuManager::DisplayType)
DEFINE_UI_CLASS_PROPERTY_TYPE(ui::ImageModel*)

DEFINE_UI_CLASS_PROPERTY_KEY(ActionAppMenuManager::DisplayType,
                             kAppMenuDisplayTypeInternal,
                             ActionAppMenuManager::DisplayType::kRow)

DEFINE_UI_CLASS_PROPERTY_KEY(ui::ColorId,
                             kAppMenuContainerColorInternal,
                             ui::kColorMenuBackground)

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::u16string, kAppMenuTextOverrideInternal)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(ui::ImageModel, kAppMenuIconOverrideInternal)

const ui::ClassProperty<ActionAppMenuManager::DisplayType>* const
    ActionAppMenuManager::kDisplayTypeKey = kAppMenuDisplayTypeInternal;

const ui::ClassProperty<ui::ColorId>* const
    ActionAppMenuManager::kContainerColorKey = kAppMenuContainerColorInternal;

const ui::ClassProperty<std::u16string*>* const
    ActionAppMenuManager::kTextOverrideKey = kAppMenuTextOverrideInternal;

const ui::ClassProperty<ui::ImageModel*>* const
    ActionAppMenuManager::kIconOverrideKey = kAppMenuIconOverrideInternal;

namespace {

// Builder helper to simplify declaring the action item structure for the app
// menu.
class AppMenuBuilder {
 public:
  using DisplayType = ActionAppMenuManager::DisplayType;

  explicit AppMenuBuilder(actions::ActionItem* parent,
                          std::optional<ui::ColorId> bg_color = std::nullopt)
      : AppMenuBuilder(static_cast<actions::BaseAction*>(parent), bg_color) {}
  explicit AppMenuBuilder(actions::BaseAction* parent,
                          std::optional<ui::ColorId> bg_color = std::nullopt)
      : parent_(parent), bg_color_(bg_color) {}
  AppMenuBuilder(const AppMenuBuilder&) = delete;
  AppMenuBuilder& operator=(const AppMenuBuilder&) = delete;
  ~AppMenuBuilder() = default;

  // Adds a standard row item.
  AppMenuBuilder& AddAction(
      actions::ActionId id,
      DisplayType type = DisplayType::kRow,
      std::optional<std::u16string> text_override = std::nullopt,
      std::optional<ui::ImageModel> icon_override = std::nullopt) {
    auto item = ActionAppMenuManager::CreateIndirectActionItem(
        id, type, bg_color_, std::move(text_override),
        std::move(icon_override));
    if (item && parent_) {
      parent_->AddChild(std::move(item));
    }
    return *this;
  }

  // Adds a section header.
  AppMenuBuilder& AddSectionHeader(int string_id) {
    auto section_item = ActionAppMenuManager::CreateSectionActionItem(
        l10n_util::GetStringUTF16(string_id), DisplayType::kRow, bg_color_);
    if (parent_) {
      parent_ = parent_->AddChild(std::move(section_item));
    }
    return *this;
  }

  // Adds a static submenu via lambda.
  AppMenuBuilder& AddSubmenu(
      actions::ActionId id,
      base::FunctionRef<void(AppMenuBuilder&)> build_submenu) {
    auto item = ActionAppMenuManager::CreateIndirectActionItem(
        id, DisplayType::kRow, bg_color_);
    if (!item || !parent_) {
      return *this;
    }
    auto* item_ptr = parent_->AddChild(std::move(item));
    AppMenuBuilder sub_builder(item_ptr);
    build_submenu(sub_builder);
    return *this;
  }

  // Adds a dynamic submenu populated at runtime.
  AppMenuBuilder& AddDynamicSubmenu(
      actions::ActionId id,
      actions::BaseAction::PopulateChildActions populate_callback,
      std::optional<base::FunctionRef<void(AppMenuBuilder&)>> build_submenu =
          std::nullopt) {
    auto item = ActionAppMenuManager::CreateIndirectActionItem(
        id, DisplayType::kRow, bg_color_);
    if (!item || !parent_) {
      return *this;
    }
    if (build_submenu.has_value()) {
      AppMenuBuilder sub_builder(item.get(), bg_color_);
      (*build_submenu)(sub_builder);
    }
    item->SetPopulateChildrenCallback(std::move(populate_callback));
    item->PopulateChildItems();
    parent_->AddChild(std::move(item));
    return *this;
  }

 private:
  raw_ptr<actions::BaseAction> parent_;
  std::optional<ui::ColorId> bg_color_;
};

}  // namespace

// Creates the Indirect Action Item which is the basis for the app menu in
// order to preserve hierarchy in action items
std::unique_ptr<actions::IndirectActionItem>
ActionAppMenuManager::CreateIndirectActionItem(
    actions::ActionId action_id,
    DisplayType display_type,
    std::optional<ui::ColorId> container_color,
    std::optional<std::u16string> text_override,
    std::optional<ui::ImageModel> icon_override) {
  actions::ActionItem* action =
      actions::ActionManager::Get().FindAction(action_id);
  if (!action) {
    return nullptr;
  }

  action->SetProperty(kDisplayTypeKey, display_type);

  if (container_color.has_value()) {
    action->SetProperty(kContainerColorKey, container_color.value());
  }

  auto item = std::make_unique<actions::IndirectActionItem>(action);

  if (text_override.has_value()) {
    item->SetProperty(kTextOverrideKey,
                      std::make_unique<std::u16string>(text_override.value()));
  }

  if (icon_override.has_value()) {
    item->SetProperty(kIconOverrideKey,
                      std::make_unique<ui::ImageModel>(icon_override.value()));
  }

  return item;
}

// Creates the Action Item for the headers of each section in the app menu
std::unique_ptr<AppMenuSectionActionItem>
ActionAppMenuManager::CreateSectionActionItem(
    std::u16string text,
    DisplayType display_type,
    std::optional<ui::ColorId> container_color) {
  auto section_item = std::make_unique<AppMenuSectionActionItem>(text);

  section_item->SetProperty(kDisplayTypeKey, display_type);

  if (container_color.has_value()) {
    section_item->SetProperty(kContainerColorKey, container_color.value());
  }

  return section_item;
}

actions::ActionItem* ActionAppMenuManager::GetAppMenuRoot(
    BrowserWindowInterface* browser_window_interface) {
  return actions::ActionManager::Get().FindAction(
      kActionAppMenuRoot,
      BrowserActions::From(browser_window_interface)->root_action_item());
}

ActionAppMenuManager::ActionAppMenuManager(
    BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface),
      recent_tabs_menu_(
          std::make_unique<RecentTabsDynamicMenu>(browser_window_interface)),
      bookmarks_menu_(
          std::make_unique<BookmarksDynamicMenu>(browser_window_interface)) {}

ActionAppMenuManager::~ActionAppMenuManager() = default;

actions::ActionItem* ActionAppMenuManager::GetAppMenuRoot() const {
  return GetAppMenuRoot(browser_window_interface_);
}

void ActionAppMenuManager::CreateMenuHierarchy() {
  actions::ActionItem* root = GetAppMenuRoot();
  if (!root) {
    return;
  }

  AddBlockHeaderActions(root);
  AddYourChromeActions(root);
  AddToolsAndActionsActions(root);
  AddFooterActions(root);
}

void ActionAppMenuManager::AddBlockHeaderActions(actions::ActionItem* root) {
  AppMenuBuilder(root)
      .AddAction(kActionNewTab, DisplayType::kBlock,
                 /*text_override=*/std::nullopt,
                 /*icon_override=*/
                 ui::ImageModel::FromVectorIcon(
                     features::IsRoundedIconsEnabled() ? kTabIcon
                                                       : kNewTabRefreshOldIcon,
                     ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize))
      .AddAction(kActionNewWindow, DisplayType::kBlock)
      .AddAction(kActionNewIncognitoWindow, DisplayType::kBlock,
                 /*text_override=*/l10n_util::GetStringUTF16(IDS_INCOGNITO));
}

void ActionAppMenuManager::AddYourChromeActions(actions::ActionItem* root) {
  AppMenuBuilder(root, kColorAppMenuYourChromeBackground)
      .AddSectionHeader(IDS_APP_MENU_YOUR_CHROME_HEADER)
      .AddAction(kActionShowPasswordManager)
      .AddDynamicSubmenu(
          kActionRecentTabsSubmenu,
          base::BindRepeating(&RecentTabsDynamicMenu::BuildRecentTabsActions,
                              recent_tabs_menu_->GetWeakPtr()))
      .AddAction(kActionShowDownloads)
      .AddAction(kActionManageExtensions)
      .AddDynamicSubmenu(
          kActionBookmarksSubmenu,
          base::BindRepeating(&BookmarksDynamicMenu::BuildBookmarksActions,
                              bookmarks_menu_->GetWeakPtr()),
          [](AppMenuBuilder& sub_builder) {
            sub_builder.AddAction(kActionBookmarkThisTab)
                .AddAction(kActionBookmarkAllTabs);
          })
      .AddAction(kActionClearBrowsingData);
}

void ActionAppMenuManager::AddToolsAndActionsActions(
    actions::ActionItem* root) {
  AppMenuBuilder builder(root, kColorAppMenuToolsAndActionsBackground);

  builder.AddSectionHeader(IDS_APP_MENU_TOOLS_AND_ACTIONS_HEADER)
      .AddAction(kActionPrint);

  if (glic::GlicEnabling::IsEnabledForProfile(
          browser_window_interface_->GetProfile())) {
    builder.AddAction(kActionOpenGlic);
  }

  builder.AddAction(kActionShowLensOverlayFromAppMenu)
      .AddAction(kActionShowTranslate);

  builder.AddSubmenu(kActionFindAndEditSubmenu, [](AppMenuBuilder& sub) {
    sub.AddAction(kActionFind)
        .AddAction(actions::kActionCut)
        .AddAction(actions::kActionCopy)
        .AddAction(actions::kActionPaste);
  });

  Profile* profile = browser_window_interface_->GetProfile();

  builder.AddSubmenu(
      kActionSaveAndShareSubmenu, [profile](AppMenuBuilder& sub) {
        if (media_router::MediaRouterEnabled(profile)) {
          sub.AddAction(kActionRouteMedia);
        }

        sub.AddAction(kActionSavePage).AddAction(kActionCreateShortcut);

        if (!sharing_hub::SharingIsDisabledByPolicy(profile)) {
          sub.AddAction(kActionCopyUrl)
              .AddAction(kActionSendTabToSelf)
              .AddAction(kActionQrCodeGenerator);
        }
        if (sharing_hub::DesktopScreenshotsFeatureEnabled(profile)) {
          sub.AddAction(kActionSharingHubScreenshot);
        }
      });

  builder.AddSubmenu(kActionDeveloperSubmenu, [](AppMenuBuilder& sub) {
    sub.AddAction(kActionTabSearch).AddAction(kActionNameWindow);

    if (tabs::IsVerticalTabsFeatureEnabled()) {
      sub.AddAction(kActionToggleVerticalTabs);
    }

    sub.AddAction(kActionSidePanelShowCustomizeChrome)
        .AddAction(kActionShowReadingModeSidePanel)
        .AddAction(kActionPerformance)
        .AddAction(kActionTaskManagerAppMenu)
        .AddAction(kActionDevTools);

    if (base::debug::IsProfilingSupported()) {
      sub.AddAction(kActionProfilingEnabled);
    }

    sub.AddAction(kActionShowChromeLabs);
  });
}

void ActionAppMenuManager::AddFooterActions(actions::ActionItem* root) {
  AppMenuBuilder(root)
      .AddAction(kActionOptions, DisplayType::kFooter)
      .AddAction(kActionHelpSubmenu, DisplayType::kFooter)
      .AddAction(kActionExit, DisplayType::kFooter);
}
