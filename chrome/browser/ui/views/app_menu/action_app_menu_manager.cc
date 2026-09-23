// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/debug/profiler.h"
#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/enterprise/isolated_mode/isolated_mode_settings_service_factory.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/command_action_updater.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "ui/base/class_property.h"
#include "ui/base/models/menu_separator_types.h"
#if BUILDFLAG(IS_CHROMEOS)
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/screen.h"
#endif
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/feedback/report_unsafe_site_dialog.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#endif
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/profiles/profile_view_utils.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_prefs.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/toolbar/reading_list_sub_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/app_menu/app_menu_zoom_view.h"
#include "chrome/browser/ui/views/app_menu/bookmarks_dynamic_menu.h"
#include "chrome/browser/ui/views/app_menu/profile_dynamic_menu.h"
#include "chrome/browser/ui/views/app_menu/recent_tabs_dynamic_menu.h"
#include "chrome/browser/ui/views/app_menu/send_tab_to_self_dynamic_menu.h"
#include "chrome/browser/ui/views/app_menu/tab_group_dynamic_menu.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "chrome/browser/ui/web_applications/web_app_ui_utils.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_page_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_bar_visibility_state.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/lens/lens_features.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/send_tab_to_self/entry_point_display_reason.h"
#include "components/send_tab_to_self/features.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/buildflags/buildflags.h"
#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_ui_util.h"
#endif
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/menus/simple_menu_model.h"

namespace {

#if BUILDFLAG(IS_CHROMEOS)
bool IsRequestingTabletSite(BrowserWindowInterface* browser) {
  if (!browser || !browser->GetTabStripModel()) {
    return false;
  }
  content::WebContents* current_tab =
      browser->GetTabStripModel()->GetActiveWebContents();
  if (!current_tab) {
    return false;
  }
  content::NavigationEntry* entry =
      current_tab->GetController().GetLastCommittedEntry();
  if (!entry) {
    return false;
  }
  return entry->GetIsOverridingUserAgent();
}
#endif

bool ArePromotionsEnabled() {
  PrefService* local_state = g_browser_process->local_state();
  return local_state && local_state->GetBoolean(prefs::kPromotionsEnabled);
}

#if !BUILDFLAG(IS_CHROMEOS)
std::u16string GetProfileName(Profile* profile) {
  if (profile->IsIncognitoProfile() ||
      profile->IsEnterpriseIsolatedModeProfile()) {
    return l10n_util::GetStringUTF16(IDS_INCOGNITO_PROFILE_MENU_TITLE);
  } else if (profile->IsGuestSession()) {
    return l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME);
  } else if (g_browser_process && g_browser_process->profile_manager()) {
    ProfileAttributesEntry* profile_attributes =
        GetProfileAttributesFromProfile(profile);
    if (profile_attributes) {
      return GetProfileMenuDisplayName(profile_attributes);
    }
  }
  return std::u16string();
}
#endif

// Builder helper to simplify declaring the action item structure for the app
// menu.
class AppMenuBuilder {
 public:
  using DisplayType = AppMenuActionItem::DisplayType;

  AppMenuBuilder(actions::ActionItem* parent,
                 actions::ActionItem* scope,
                 std::optional<ui::ColorId> bg_color = std::nullopt,
                 DisplayType default_display_type = DisplayType::kRow)
      : AppMenuBuilder(static_cast<actions::BaseAction*>(parent),
                       scope,
                       bg_color,
                       default_display_type) {}
  AppMenuBuilder(actions::BaseAction* parent,
                 actions::ActionItem* scope,
                 std::optional<ui::ColorId> bg_color = std::nullopt,
                 DisplayType default_display_type = DisplayType::kRow)
      : parent_(parent),
        scope_(scope),
        bg_color_(bg_color),
        default_display_type_(default_display_type) {}
  AppMenuBuilder(const AppMenuBuilder&) = delete;
  AppMenuBuilder& operator=(const AppMenuBuilder&) = delete;
  ~AppMenuBuilder() = default;

  // Adds an action item.
  AppMenuBuilder& AddAction(actions::ActionId id,
                            AppMenuActionItem::ActionParams params = {}) {
    if (!params.container_color.has_value()) {
      params.container_color = bg_color_;
    }
    if (!params.display_type.has_value()) {
      params.display_type = default_display_type_;
    }
    auto item =
        AppMenuActionItem::CreateIndirect(id, scope_, std::move(params));
    if (item && parent_) {
      parent_->AddChild(std::move(item));
    }
    return *this;
  }

  // Adds a header item to the current parent without modifying the parent.
  AppMenuBuilder& AddHeader(int string_id) {
    auto header_item = AppMenuActionItem::CreateHeader(
        l10n_util::GetStringUTF16(string_id), bg_color_);
    if (parent_) {
      parent_->AddChild(std::move(header_item));
    }
    return *this;
  }

  AppMenuBuilder& AddDivider(
      ui::MenuSeparatorType type = ui::NORMAL_SEPARATOR) {
    auto item = AppMenuActionItem::CreateDivider(type);
    if (parent_) {
      parent_->AddChild(std::move(item));
    }
    return *this;
  }

  // Adds a static submenu via lambda.
  AppMenuBuilder& AddSubmenu(
      actions::ActionId id,
      base::FunctionRef<void(AppMenuBuilder&)> build_submenu,
      AppMenuActionItem::ActionParams params = {}) {
    if (!params.container_color.has_value()) {
      params.container_color = bg_color_;
    }
    if (!params.display_type.has_value()) {
      params.display_type = default_display_type_;
    }
    auto item =
        AppMenuActionItem::CreateIndirect(id, scope_, std::move(params));
    if (!item || !parent_) {
      return *this;
    }
    auto* item_ptr = parent_->AddChild(std::move(item));
    AppMenuBuilder sub_builder(item_ptr, scope_);
    build_submenu(sub_builder);
    return *this;
  }

  // Adds a structural section container, optionally populated via lambda.
  AppMenuBuilder& AddSection(
      DisplayType display_type,
      std::optional<base::FunctionRef<void(AppMenuBuilder&)>> build_section =
          std::nullopt,
      std::optional<ui::ColorId> bg_color = std::nullopt) {
    if (!parent_) {
      return *this;
    }
    const std::optional<ui::ColorId> section_bg_color =
        bg_color.has_value() ? bg_color : bg_color_;
    auto item = actions::ActionItem::Builder().Build();
    item->SetProperty(AppMenuActionItem::kDisplayTypeKey, display_type);
    if (section_bg_color.has_value()) {
      item->SetProperty(AppMenuActionItem::kContainerColorKey,
                        section_bg_color.value());
    }
    auto* item_ptr = parent_->AddChild(std::move(item));
    if (build_section.has_value()) {
      AppMenuBuilder section_builder(item_ptr, scope_, section_bg_color);
      (*build_section)(section_builder);
    }
    return *this;
  }

  // Adds a dynamic submenu populated at runtime.
  AppMenuBuilder& AddDynamicSubmenu(
      actions::ActionId id,
      actions::BaseAction::PopulateChildActions populate_callback,
      std::optional<base::FunctionRef<void(AppMenuBuilder&)>> build_submenu =
          std::nullopt,
      AppMenuActionItem::ActionParams params = {}) {
    if (!params.container_color.has_value()) {
      params.container_color = bg_color_;
    }
    auto item =
        AppMenuActionItem::CreateIndirect(id, scope_, std::move(params));
    if (!item || !parent_) {
      return *this;
    }
    if (build_submenu.has_value()) {
      AppMenuBuilder sub_builder(item.get(), scope_);
      (*build_submenu)(sub_builder);
    }

    item->SetPopulateChildrenCallback(std::move(populate_callback));
    item->PopulateChildItems();
    parent_->AddChild(std::move(item));
    return *this;
  }

  // Adds a dynamic section directly into the current parent item via callback.
  AppMenuBuilder& AddDynamicSection(
      base::FunctionRef<void(actions::BaseAction*)> build_section) {
    if (parent_) {
      build_section(parent_);
    }
    return *this;
  }

 private:
  raw_ptr<actions::BaseAction> parent_;
  raw_ptr<actions::ActionItem> scope_;
  std::optional<ui::ColorId> bg_color_;
  DisplayType default_display_type_ = DisplayType::kRow;
};

}  // namespace

ActionAppMenuManager::ActionAppMenuManager(
    BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface),
      recent_tabs_menu_(
          std::make_unique<RecentTabsDynamicMenu>(browser_window_interface)),
      bookmarks_menu_(
          std::make_unique<BookmarksDynamicMenu>(browser_window_interface)),
      tab_groups_menu_(
          std::make_unique<TabGroupDynamicMenu>(browser_window_interface)),
      send_tab_to_self_menu_(
          std::make_unique<SendTabToSelfDynamicMenu>(browser_window_interface)),
      profile_menu_(
          std::make_unique<ProfileDynamicMenu>(browser_window_interface)) {}

ActionAppMenuManager::~ActionAppMenuManager() = default;

actions::ActionItem* ActionAppMenuManager::GetAppMenuRoot() const {
  return actions::ActionManager::Get().FindAction(
      kActionAppMenuRoot,
      BrowserActions::From(browser_window_interface_)->root_action_item());
}

void ActionAppMenuManager::CreateMenuHierarchy() {
  actions::ActionItem* root = GetAppMenuRoot();
  if (!root) {
    return;
  }

  AddNotificationActions(root);
  AddSearchBarAction(root);
  AddBlockHeaderActions(root);
  AddYourChromeActions(root);
  AddToolsAndActionsActions(root);
  AddFooterActions(root);
}

void ActionAppMenuManager::AddNotificationActions(actions::ActionItem* root) {
  actions::ActionItem* scope =
      BrowserActions::From(browser_window_interface_)->root_action_item();
  AppMenuBuilder(root, scope, ui::kColorAppMenuUpgradeRowBackground)
      .AddSection(DisplayType::kSection, [this,
                                          scope](AppMenuBuilder& section) {
        bool has_notification = false;

        // Helper for adding a notification if its action is visible, with an
        // optional leading spacer if there is another notification already
        // added.
        auto maybe_add_notification =
            [&section, scope, &has_notification](
                actions::ActionId id,
                AppMenuActionItem::ActionParams params = {}) {
              actions::ActionItem* action =
                  actions::ActionManager::Get().FindAction(id, scope);
              CHECK(action);
              if (!action->GetVisible()) {
                return false;
              }
              if (has_notification) {
                section.AddDivider(ui::MenuSeparatorType::SPACING_SEPARATOR);
              }
              params.display_type = DisplayType::kNotification;
              section.AddAction(id, std::move(params));
              has_notification = true;
              return true;
            };

        maybe_add_notification(
            kActionUpgradeDialog,
            {.minor_text = AppMenuModel::GetUpgradeDialogSubstringText()});

        // At most one non-upgrade notification item (Safety Hub, Global
        // Error, or Default Browser, which are ordered by priority) should
        // be shown at a time. Return early as soon as the first one is added.

        // Query for the correct safety hub notification (if any) rather than
        // adding all potential actions here and allowing their visibility to
        // be determined by the action itself because
        // safety_hub_service->GetNotificationToShow() carries side effects and
        // is intended to be called once per menu show.
        if (auto* safety_hub_service =
                SafetyHubMenuNotificationServiceFactory::GetForProfile(
                    browser_window_interface_->GetProfile())) {
          if (std::optional<MenuNotificationEntry> notification =
                  safety_hub_service->GetNotificationToShow()) {
            if (std::optional<actions::ActionId> action_id =
                    chrome::CommandActionUpdater::GetActionId(
                        notification->command)) {
              if (maybe_add_notification(
                      action_id.value(),
                      {.text_override = notification->label})) {
                return;
              }
            }
          }
        }

        if (maybe_add_notification(kActionGlobalError)) {
          return;
        }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
        maybe_add_notification(
            kActionSetBrowserAsDefault,
            {.element_id = AppMenuModel::kSetBrowserAsDefaultMenuItem});
#endif
      });
}

void ActionAppMenuManager::AddSearchBarAction(actions::ActionItem* root) {
  if (base::FeatureList::IsEnabled(features::kChroMenuSearch)) {
    AppMenuBuilder(
        root,
        BrowserActions::From(browser_window_interface_)->root_action_item())
        .AddSection(DisplayType::kSearch);
  }
}

void ActionAppMenuManager::AddBlockHeaderActions(actions::ActionItem* root) {
  AppMenuBuilder(
      root, BrowserActions::From(browser_window_interface_)->root_action_item())
      .AddSection(DisplayType::kBlock, [this](AppMenuBuilder& section) {
        Profile* profile = browser_window_interface_->GetProfile();
        std::optional<std::u16string> new_tab_text_override;
        if (profile->IsEnterpriseIsolatedModeProfile()) {
          new_tab_text_override = BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_NEW_ISOLATED_TAB));
        } else if (profile->IsIncognitoProfile()) {
          new_tab_text_override = BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_NEW_INCOGNITO_TAB));
        }
        section
            .AddAction(
                kActionNewTab,
                {.display_type = DisplayType::kBlock,
                 .text_override = new_tab_text_override,
                 .icon_override = ui::ImageModel::FromVectorIcon(
                     features::IsRoundedIconsEnabled() ? kTabIcon
                                                       : kNewTabRefreshOldIcon,
                     ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize)})
            .AddAction(kActionNewWindow, {.display_type = DisplayType::kBlock});

        if (!profile->IsGuestSession()) {
          if (enterprise_isolated_mode::IsolatedModeReplacesIncognito(
                  profile)) {
            section.AddAction(
                kActionNewIsolatedWindow,
                {.display_type = DisplayType::kBlock,
                 .text_override = l10n_util::GetStringUTF16(IDS_ISOLATED)});
          } else {
            section.AddAction(
                kActionNewIncognitoWindow,
                {.display_type = DisplayType::kBlock,
                 .text_override = l10n_util::GetStringUTF16(IDS_INCOGNITO),
                 .element_id = AppMenuModel::kIncognitoMenuItem});
          }
        }
      });
}

void ActionAppMenuManager::AddYourChromeActions(actions::ActionItem* root) {
  AppMenuBuilder(
      root, BrowserActions::From(browser_window_interface_)->root_action_item(),
      kColorAppMenuYourChromeBackground)
      .AddSection(DisplayType::kSection, [this](AppMenuBuilder& section) {
        section.AddHeader(IDS_APP_MENU_YOUR_CHROME_HEADER);

        Profile* profile = browser_window_interface_->GetProfile();

#if !BUILDFLAG(IS_CHROMEOS)
        std::u16string profile_name = GetProfileName(profile);
        section.AddSubmenu(
            kActionProfileSubmenu,
            [this, profile](AppMenuBuilder& sub) {
              sub.AddDynamicSection([this](actions::BaseAction* parent) {
                   profile_menu_->BuildSyncSection(parent);
                 })
                  .AddAction(kActionManageGoogleAccount)
                  .AddAction(kActionCustomizeChrome)
                  .AddAction(
                      kActionCloseProfile,
                      {.text_override = l10n_util::GetPluralStringFUTF16(
                           IDS_CLOSE_PROFILE, CountBrowsersFor(profile))})
                  .AddDynamicSection([this](actions::BaseAction* parent) {
                    profile_menu_->BuildOtherProfiles(parent);
                  })
                  .AddAction(kActionAddNewProfile)
                  .AddAction(
                      kActionOpenGuestProfile,
                      {.element_id = AppMenuModel::kProfileOpenGuestItem})
                  .AddAction(kActionManageChromeProfiles);
            },
            {.text_override = profile_name.empty()
                                  ? std::nullopt
                                  : std::make_optional(std::move(profile_name)),
             .element_id = AppMenuModel::kProfileMenuItem});
#endif

        if (!profile->IsGuestSession()) {
          section.AddSubmenu(
              kActionPasswordsAndAutofillSubmenu,
              [](AppMenuBuilder& sub) {
                sub.AddAction(
                       kActionShowPasswordManager,
                       {.element_id = AppMenuModel::kPasswordManagerMenuItem})
                    .AddAction(kActionShowPaymentMethods)
                    .AddAction(
                        kActionShowContactInfo,
                        {.element_id = AppMenuModel::kContactInfoMenuItem})
                    .AddAction(
                        kActionShowIdentityDocs,
                        {.element_id = AppMenuModel::kIdentityDocsMenuItem})
                    .AddAction(kActionShowTravel,
                               {.element_id = AppMenuModel::kTravelMenuItem});
              },
              {.element_id = AppMenuModel::kPasswordAndAutofillMenuItem});
        }

        if (!profile->IsOffTheRecord()) {
          section.AddDynamicSubmenu(
              kActionRecentTabsSubmenu,
              base::BindRepeating(
                  &RecentTabsDynamicMenu::BuildRecentTabsActions,
                  recent_tabs_menu_->GetWeakPtr()),
              /*build_submenu=*/std::nullopt,
              {.element_id = AppMenuModel::kHistoryMenuItem});
        }

        section.AddAction(kActionShowDownloadsPage,
                          {.element_id = AppMenuModel::kDownloadsMenuItem});

        if (!profile->IsGuestSession()) {
          section.AddDynamicSubmenu(
              kActionBookmarksSubmenu,
              base::BindRepeating(&BookmarksDynamicMenu::BuildBookmarksActions,
                                  bookmarks_menu_->GetWeakPtr()),
              [this](AppMenuBuilder& sub_builder) {
                sub_builder.AddAction(kActionBookmarkThisTab)
                    .AddAction(kActionBookmarkAllTabs)
                    .AddDivider();

                Profile* profile = browser_window_interface_->GetProfile();
                if (base::FeatureList::IsEnabled(
                        ntp_features::kNtpSimplificationBookmarkBar)) {
                  sub_builder.AddSubmenu(
                      kActionBookmarkBarSubmenu,
                      [](AppMenuBuilder& bar_sub) {
                        bar_sub
                            .AddAction(kActionBookmarkBarSubmenuAlwaysHide,
                                       {.is_checkable = true})
                            .AddAction(kActionBookmarkBarSubmenuAlwaysShow,
                                       {.is_checkable = true})
                            .AddAction(kActionBookmarkBarSubmenuOnlyOnNtp,
                                       {.is_checkable = true});
                      },
                      {.element_id =
                           BookmarkSubMenuModel::kShowBookmarkBarMenuItem});
                } else {
                  const int bookmark_bar_string_id =
                      profile->GetPrefs()->GetBoolean(
                          bookmarks::prefs::kShowBookmarkBar)
                          ? IDS_HIDE_BOOKMARK_BAR
                          : IDS_SHOW_BOOKMARK_BAR;
                  sub_builder.AddAction(
                      kActionShowBookmarkBar,
                      {.text_override =
                           l10n_util::GetStringUTF16(bookmark_bar_string_id),
                       .element_id =
                           BookmarkSubMenuModel::kShowBookmarkBarMenuItem});
                }

                sub_builder.AddAction(
                    kActionSidePanelShowBookmarks,
                    {.text_override = l10n_util::GetStringUTF16(
                         IDS_SHOW_BOOKMARK_SIDE_PANEL),
                     .element_id =
                         BookmarkSubMenuModel::kShowBookmarkSidePanelItem});

                const int bookmark_manager_string_id =
                    features::IsMenuSimplificationEnabled()
                        ? IDS_BOOKMARK_MANAGER_V2
                        : IDS_BOOKMARK_MANAGER;
                sub_builder.AddAction(
                    kActionShowBookmarkManager,
                    {.text_override = l10n_util::GetStringUTF16(
                         bookmark_manager_string_id)});

#if !BUILDFLAG(IS_CHROMEOS)
                sub_builder.AddAction(kActionImportSettings);
#endif

                sub_builder.AddDivider();

                sub_builder.AddSubmenu(
                    kActionReadingListSubmenu,
                    [](AppMenuBuilder& reading_list_sub) {
                      reading_list_sub.AddAction(kActionReadingListMenuAddTab)
                          .AddAction(
                              kActionSidePanelShowReadingList,
                              {.text_override = l10n_util::GetStringUTF16(
                                   IDS_READING_LIST_MENU_SHOW_UI),
                               .element_id = ReadingListSubMenuModel::
                                   kReadingListMenuShowUI});
                    },
                    {.element_id = BookmarkSubMenuModel::kReadingListMenuItem});
              },
              {.element_id = AppMenuModel::kBookmarksMenuItem});
        }

        if (profile->IsRegularProfile()) {
          section.AddDynamicSubmenu(
              kActionSavedTabGroupsSubmenu,
              base::BindRepeating(&TabGroupDynamicMenu::BuildTabGroupsAction,
                                  tab_groups_menu_->GetWeakPtr()),
              [](AppMenuBuilder& sub) {
                sub.AddAction(
                    kActionCreateNewTabGroup,
                    {.element_id =
                         tab_groups::STGEverythingMenu::kCreateNewTabGroup});
              },
              {.element_id = AppMenuModel::kTabGroupsMenuItem});
        }

#if BUILDFLAG(ENABLE_EXTENSIONS)
        if (ArePromotionsEnabled() &&
            base::FeatureList::IsEnabled(
                features::kExtensionsCollapseMainMenu) &&
            !extensions::ui_util::HasManageableExtensions(profile)) {
          section.AddAction(
              kActionFindExtensions,
              {.element_id =
                   ExtensionsMenuModel::kVisitChromeWebStoreMenuItem});
        } else {
          section.AddSubmenu(
              kActionExtensionsSubmenu,
              [](AppMenuBuilder& sub) {
                sub.AddAction(
                       kActionExtensionsSubmenuManageExtensions,
                       {.element_id =
                            ExtensionsMenuModel::kManageExtensionsMenuItem})
                    .AddAction(kActionExtensionsSubmenuVisitChromeWebStore,
                               {.element_id = ExtensionsMenuModel::
                                    kVisitChromeWebStoreMenuItem});
              },
              {.element_id = AppMenuModel::kExtensionsMenuItem});
        }
#endif

        section.AddAction(
            kActionClearBrowsingData,
            {.element_id = AppMenuModel::kClearBrowsingDataMenuItem});
      });
}

void ActionAppMenuManager::AddToolsAndActionsActions(
    actions::ActionItem* root) {
  AppMenuBuilder(
      root, BrowserActions::From(browser_window_interface_)->root_action_item(),
      kColorAppMenuToolsAndActionsBackground)
      .AddSection(DisplayType::kSection, [this](AppMenuBuilder& section) {
        section.AddHeader(IDS_APP_MENU_TOOLS_AND_ACTIONS_HEADER)
            .AddSubmenu(
                kActionZoomSubmenu,
                [](AppMenuBuilder& sub) {
                  sub.AddAction(kActionZoomMinus)
                      .AddAction(kActionZoomNormal)
                      .AddAction(kActionZoomPlus)
                      .AddAction(kActionFullscreen);
                },
                {.display_type = DisplayType::kCustom,
                 .item_height = AppMenuActionItem::ItemHeight::kExpanded})
            .AddDivider(ui::MenuSeparatorType::SPACING_SEPARATOR)
            .AddAction(kActionPrint);

        Profile* profile = browser_window_interface_->GetProfile();

        if (glic::GlicEnabling::IsEnabledForProfile(profile)) {
          section.AddAction(
              kActionOpenGlic,
              {.new_badge_feature = &features::kGlicAppMenuNewBadge});
        }

        if (auto* controller = lens::LensOverlayEntryPointController::From(
                browser_window_interface_);
            controller && controller->IsEnabled()) {
          section.AddAction(kActionShowLensOverlayFromAppMenu,
                            {.new_badge_feature = &lens::features::kLensOverlay,
                             .element_id = AppMenuModel::kShowLensOverlay});
        }

        section.AddAction(kActionShowTranslate);

        section.AddSubmenu(kActionFindAndEditSubmenu, [](AppMenuBuilder& sub) {
          sub.AddAction(kActionFind)
              .AddDivider()
              .AddAction(actions::kActionCut)
              .AddAction(actions::kActionCopy)
              .AddAction(actions::kActionPaste);
        });

        const int save_and_share_string_id =
            media_router::MediaRouterEnabled(profile)
                ? IDS_CAST_SAVE_AND_SHARE_MENU
                : IDS_SAVE_AND_SHARE_MENU;

        section.AddSubmenu(
            kActionSaveAndShareSubmenu,
            [this](AppMenuBuilder& sub) {
              Profile* profile = browser_window_interface_->GetProfile();
              if (media_router::MediaRouterEnabled(profile)) {
                sub.AddHeader(IDS_SAVE_AND_SHARE_MENU_CAST)
                    .AddAction(kActionRouteMedia)
                    .AddDivider();
              }

              sub.AddHeader(IDS_SAVE_AND_SHARE_MENU_SAVE)
                  .AddAction(kActionSavePage)
                  .AddDivider();

              if (std::u16string install_item = web_app::GetInstallPWALabel(
                      browser_window_interface_.get());
                  !install_item.empty()) {
                sub.AddAction(kActionInstallPwa,
                              {.text_override = install_item,
                               .icon_override = web_app::GetInstallPWAIcon(
                                   browser_window_interface_.get()),
                               .element_id = AppMenuModel::kInstallAppItem});
              } else if (std::u16string open_item = web_app::GetOpenPWALabel(
                             browser_window_interface_.get());
                         !open_item.empty()) {
                sub.AddAction(kActionOpenInPwaWindow,
                              {.text_override = open_item,
                               .icon_override = ui::ImageModel::FromVectorIcon(
                                   features::IsRoundedIconsEnabled()
                                       ? kDesktopWindowsIcon
                                       : kDesktopWindowsChromeRefreshOldIcon,
                                   ui::kColorMenuIcon,
                                   ui::SimpleMenuModel::kDefaultIconSize)});
              }

              sub.AddAction(kActionCreateShortcut,
                            {.element_id = AppMenuModel::kCreateShortcutItem});

              if (!sharing_hub::SharingIsDisabledByPolicy(profile) ||
                  sharing_hub::DesktopScreenshotsFeatureEnabled(profile)) {
                sub.AddDivider();
                sub.AddHeader(IDS_SAVE_AND_SHARE_MENU_SHARE);
                if (!sharing_hub::SharingIsDisabledByPolicy(profile)) {
                  sub.AddAction(kActionCopyUrl);

                  content::WebContents* web_contents =
                      browser_window_interface_->GetTabStripModel()
                          ? browser_window_interface_->GetTabStripModel()
                                ->GetActiveWebContents()
                          : nullptr;
                  std::optional<send_tab_to_self::EntryPointDisplayReason>
                      reason =
                          web_contents
                              ? send_tab_to_self::GetEntryPointDisplayReason(
                                    web_contents)
                              : std::nullopt;

                  if (web_contents &&
                      base::FeatureList::IsEnabled(
                          send_tab_to_self::
                              kSendTabToSelfEnhancedDesktopUIv2) &&
                      reason == send_tab_to_self::EntryPointDisplayReason::
                                    kOfferFeature) {
                    sub.AddDynamicSubmenu(
                        kActionSendTabToSelf,
                        base::BindRepeating(
                            &SendTabToSelfDynamicMenu::
                                BuildSendTabToSelfActions,
                            send_tab_to_self_menu_->GetWeakPtr()),
                        /*build_submenu=*/std::nullopt,
                        {.new_badge_feature =
                             &send_tab_to_self::
                                 kSendTabToSelfEnhancedDesktopUIv2});
                  } else {
                    sub.AddAction(kActionSendTabToSelf);
                  }

                  sub.AddAction(kActionQrCodeGenerator);
                }
                if (sharing_hub::DesktopScreenshotsFeatureEnabled(profile)) {
                  sub.AddAction(kActionSharingHubScreenshot);
                }
              }
            },
            {.text_override =
                 l10n_util::GetStringUTF16(save_and_share_string_id),
             .element_id = AppMenuModel::kSaveAndShareMenuItem});

#if BUILDFLAG(IS_CHROMEOS)
        if (display::Screen::Get()->InTabletMode()) {
          section.AddAction(
              kActionToggleRequestTabletSite,
              {.text_override =
                   l10n_util::GetStringUTF16(IDS_TOGGLE_REQUEST_TABLET_SITE),
               .icon_override = ui::ImageModel::FromVectorIcon(
                   IsRequestingTabletSite(browser_window_interface_.get())
                       ? (features::IsRoundedIconsEnabled()
                              ? kMobileCheckIcon
                              : kRequestMobileSiteCheckedOldIcon)
                       : (features::IsRoundedIconsEnabled()
                              ? kMobileIcon
                              : kRequestMobileSiteUncheckedOldIcon),
                   ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize)});
        }
#endif

        section.AddSubmenu(
            kActionDeveloperSubmenu,
            [this](AppMenuBuilder& sub) {
#if BUILDFLAG(IS_CHROMEOS)
              const bool is_tablet_mode =
                  display::Screen::Get()->InTabletMode();
#else
              const bool is_tablet_mode = false;
#endif
              if (!is_tablet_mode) {
                sub.AddAction(kActionTabSearch,
                              {.icon_override = ui::ImageModel::FromVectorIcon(
                                   features::IsRoundedIconsEnabled()
                                       ? kManageSearchIcon
                                       : kTabSearchTabStripOldIcon)});
              }

              sub.AddAction(kActionNameWindow);

              if (auto* controller =
                      tabs::VerticalTabStripStateController::From(
                          browser_window_interface_.get())) {
                if (controller->ShouldDisplayVerticalTabs()) {
                  sub.AddAction(
                      kActionToggleVerticalTabs,
                      {.text_override = l10n_util::GetStringUTF16(
                           IDS_SWITCH_TO_HORIZONTAL_TAB),
                       .icon_override = ui::ImageModel::FromVectorIcon(
                           features::IsRoundedIconsEnabled() ? kToolbarIcon
                                                             : kToolbarOldIcon,
                           ui::kColorMenuIcon,
                           ui::SimpleMenuModel::kDefaultIconSize)});
                } else {
                  sub.AddAction(
                      kActionToggleVerticalTabs,
                      {.text_override = l10n_util::GetStringUTF16(
                           IDS_SWITCH_TO_VERTICAL_TAB),
                       .icon_override = ui::ImageModel::FromVectorIcon(
                           base::i18n::IsRTL()
                               ? (features::IsRoundedIconsEnabled()
                                      ? kDockToLeftIcon
                                      : kDockToRightOldIcon)
                               : (features::IsRoundedIconsEnabled()
                                      ? kDockToRightIcon
                                      : kDockToLeftOldIcon),
                           ui::kColorMenuIcon,
                           ui::SimpleMenuModel::kDefaultIconSize),
                       .new_badge_feature = &tabs::kVerticalTabsNewBadge});
                }
              }

              Profile* profile = browser_window_interface_->GetProfile();
              if (CustomizeChromePageHandler::IsSupported(
                      NtpCustomBackgroundServiceFactory::GetForProfile(profile),
                      profile)) {
                sub.AddAction(kActionSidePanelShowCustomizeChrome);
              }

              sub.AddDivider()
                  .AddAction(
                      kActionShowReadingModeSidePanel,
                      {.element_id = ToolsMenuModel::kReadingModeMenuItem})
                  .AddDivider()
                  .AddAction(
                      kActionPerformance,
                      {.element_id = ToolsMenuModel::kPerformanceMenuItem})
                  .AddAction(kActionTaskManagerAppMenu);

#if BUILDFLAG(IS_CHROMEOS)
              sub.AddAction(kActionTakeScreenshot);
#endif

              sub.AddDivider().AddAction(kActionDevTools);

              if (base::debug::IsProfilingSupported()) {
                sub.AddDivider().AddAction(kActionProfilingEnabled,
                                           {.is_checkable = true});
              }

              if (IsChromeLabsEnabled()) {
                UpdateChromeLabsNewBadgePrefs(profile);
                if (ShouldShowChromeLabsUI(profile) &&
                    profile->GetPrefs()->GetBoolean(
                        chrome_labs_prefs::
                            kBrowserLabsEnabledEnterprisePolicy)) {
                  sub.AddDivider().AddAction(
                      kActionShowChromeLabs,
                      {.element_id = ToolsMenuModel::kChromeLabsMenuItem});
                }
              }
            },
            {.element_id = AppMenuModel::kMoreToolsMenuItem});
      });
}

void ActionAppMenuManager::AddFooterActions(actions::ActionItem* root) {
  AppMenuBuilder(
      root, BrowserActions::From(browser_window_interface_)->root_action_item())
      .AddSection(DisplayType::kFooter, [browser_window_interface =
                                             browser_window_interface_.get()](
                                            AppMenuBuilder& section) {
        section.AddAction(kActionOptions);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        section.AddSubmenu(
            kActionHelpSubmenu,
            [browser_window_interface](AppMenuBuilder& sub) {
              sub.AddAction(kActionAbout);

              if (whats_new::IsEnabled()) {
                sub.AddAction(kActionChromeWhatsNew);
              }

#if BUILDFLAG(IS_CHROMEOS) && defined(OFFICIAL_BUILD)
              sub.AddAction(
                  kActionHelpPageViaMenu,
                  {.text_override = l10n_util::GetStringUTF16(IDS_GET_HELP)});
#else
              sub.AddAction(kActionHelpPageViaMenu);
#endif

              Profile* profile = browser_window_interface->GetProfile();
              if (chrome::CanShowFeedback(profile)) {
                sub.AddAction(kActionFeedback);

                if (feedback::ReportUnsafeSiteDialog::IsEnabled(*profile)) {
                  sub.AddAction(
                      kActionReportUnsafeSite,
                      {.element_id = HelpMenuModel::kReportUnsafeSiteMenuItem});
                }
              }
            },
            {.element_id = AppMenuModel::kHelpMenuItem});
#else
        section.AddAction(kActionAbout);
#endif

        if (browser_defaults::kShowExitMenuItem) {
          section.AddAction(kActionExit);
        }

#if !BUILDFLAG(IS_CHROMEOS)
        Profile* profile = browser_window_interface->GetProfile();
        if (ShouldDisplayManagedUi(profile)) {
          section.AddAction(
              kActionShowManagementPage,
              {.display_type = DisplayType::kRow,
               .text_override = GetManagedUiMenuItemLabel(profile),
               .icon_override =
                   ui::ImageModel::FromVectorIcon(GetManagedUiIcon(profile))});
        }
#endif  // !BUILDFLAG(IS_CHROMEOS)
      });
}
