// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_menu_model.h"

#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_context_menu_delegate.h"
#include "chrome/browser/ui/tabs/existing_tab_group_sub_menu_model.h"
#include "chrome/browser/ui/tabs/existing_window_sub_menu_model.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/glic_tab_sub_menu_model.h"
#include "chrome/browser/ui/tabs/split_tab_menu_model.h"
#include "chrome/browser/ui/tabs/split_tab_swap_menu_model.h"
#include "chrome/browser/ui/tabs/split_view_layout_menu_model.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/feed/feed_feature_list.h"
#include "components/send_tab_to_self/features.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/vector_icons.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "extensions/common/extension_features.h"
#endif

using base::UserMetricsAction;

namespace {
constexpr int kTabMenuIconSize = 16;
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabMenuModel, kAddANoteTabMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabMenuModel, kSplitTabsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabMenuModel, kArrangeSplitTabsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabMenuModel, kSwapSplitTabsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabMenuModel, kAddNewTabAdjacentMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabMenuModel, kDuplicateMenuItem);

TabMenuModel::TabMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                           TabMenuModelDelegate* tab_menu_model_delegate,
                           TabStripModel* tab_strip,
                           int index)
    : ui::SimpleMenuModel(delegate),
      tab_strip_(tab_strip),
      tab_menu_model_delegate_(tab_menu_model_delegate),
      tab_interface_(tab_strip_->GetTabAtIndex(index)->GetWeakPtr()) {
  CHECK(tab_strip_);

  if (tab_strip_->delegate()->IsForWebApp()) {
    BuildForWebApp(index);
  } else {
    Build(index);
  }
}

TabMenuModel::~TabMenuModel() = default;

void TabMenuModel::BuildForWebApp(int index) {
  AddItemWithStringId(TabStripModel::CommandCopyURL, IDS_COPY_URL);
  AddItemWithStringId(TabStripModel::CommandReload, IDS_TAB_CXMENU_RELOAD);
  AddItemWithStringId(TabStripModel::CommandGoBack, IDS_CONTENT_CONTEXT_BACK);

  if (!web_app::IsPinnedHomeTab(tab_strip_, index) &&
      (!web_app::HasPinnedHomeTab(tab_strip_) ||
       !tab_strip_->selection_model().IsSelected(*tab_strip_->begin()))) {
    int num_tabs = tab_strip_->selection_model().size();
    if (ExistingWindowSubMenuModel::ShouldShowSubmenuForApp(
            tab_menu_model_delegate_)) {
      // Create submenu with existing windows
      add_to_existing_window_submenu_ = ExistingWindowSubMenuModel::Create(
          delegate(), tab_menu_model_delegate_, tab_strip_, index);
      AddSubMenu(TabStripModel::CommandMoveToExistingWindow,
                 l10n_util::GetPluralStringFUTF16(
                     IDS_TAB_CXMENU_MOVETOANOTHERWINDOW, num_tabs),
                 add_to_existing_window_submenu_.get());
    } else {
      AddItem(TabStripModel::CommandMoveTabsToNewWindow,
              l10n_util::GetPluralStringFUTF16(
                  IDS_TAB_CXMENU_MOVE_TABS_TO_NEW_WINDOW, num_tabs));
    }
  }

  AddSeparator(ui::NORMAL_SEPARATOR);

  if (!web_app::IsPinnedHomeTab(tab_strip_, index)) {
    AddItemWithStringId(TabStripModel::CommandCloseTab,
                        IDS_TAB_CXMENU_CLOSETAB);
    AddItemWithStringId(TabStripModel::CommandCloseOtherTabs,
                        IDS_TAB_CXMENU_CLOSEOTHERTABS);
  }
  if (web_app::HasPinnedHomeTab(tab_strip_)) {
    AddItemWithStringId(TabStripModel::CommandCloseAllTabs,
                        IDS_TAB_CXMENU_CLOSEALLTABS);
  }
}

void TabMenuModel::BuildSendTabToSelfSubmenu(int index) {
  send_tab_to_self_submenu_delegate_ =
      std::make_unique<send_tab_to_self::SendTabToSelfContextMenuDelegate>(
          tab_strip_->GetWebContentsAt(index));
  send_tab_to_self_submenu_ = std::make_unique<ui::SimpleMenuModel>(
      send_tab_to_self_submenu_delegate_.get());

  send_tab_to_self_submenu_delegate_->PopulateSubmenu(
      send_tab_to_self_submenu_.get());

#if BUILDFLAG(IS_MAC)
  AddSubMenuWithStringId(TabStripModel::CommandSendTabToSelf,
                         IDS_MENU_SEND_TAB_TO_SELF,
                         send_tab_to_self_submenu_.get());
#else
  AddSubMenuWithStringIdAndIcon(
      TabStripModel::CommandSendTabToSelf, IDS_MENU_SEND_TAB_TO_SELF,
      send_tab_to_self_submenu_.get(),
      ui::ImageModel::FromVectorIcon(kDevicesIcon, ui::kColorMenuIcon,
                                     kTabMenuIconSize));
#endif
}

void TabMenuModel::BuildLegacySendTabToSelfItem() {
#if BUILDFLAG(IS_MAC)
  AddItem(TabStripModel::CommandSendTabToSelf,
          l10n_util::GetStringUTF16(IDS_MENU_SEND_TAB_TO_SELF));
#else
  AddItemWithIcon(TabStripModel::CommandSendTabToSelf,
                  l10n_util::GetStringUTF16(IDS_MENU_SEND_TAB_TO_SELF),
                  ui::ImageModel::FromVectorIcon(kDevicesIcon));
#endif
}

void TabMenuModel::AppendGlicItems(int index,
                                   int num_tabs,
                                   const std::vector<int>& indices) {
  glic_tab_sub_menu_model_ =
      std::make_unique<glic::GlicTabSubMenuModel>(tab_strip_, index);

  if (base::FeatureList::IsEnabled(features::kMenuSimplification)) {
    AddSubMenuWithIcon(TabStripModel::CommandGlicShare,
                       l10n_util::GetPluralStringFUTF16(
                           IDS_TAB_CXMENU_GLIC_START_SHARE, num_tabs),
                       glic_tab_sub_menu_model_.get(),
                       ui::ImageModel::FromVectorIcon(
                           glic::GlicVectorIconManager::GetVectorIcon(
                               IDR_GLIC_BUTTON_VECTOR_ICON)));
  } else {
    AddSubMenu(TabStripModel::CommandGlicShare,
               l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_GLIC_START_SHARE,
                                                num_tabs),
               glic_tab_sub_menu_model_.get());
  }

  auto* service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(tab_strip_->profile());
  CHECK(service);
  if (std::ranges::any_of(indices, [&](int index) {
        return service->instance_coordinator().IsTabPinnedToAnyInstance(
            tab_strip_->GetTabAtIndex(index)->GetHandle());
      })) {
    AddItem(TabStripModel::CommandGlicUnshare,
            l10n_util::GetStringUTF16(IDS_TAB_CXMENU_GLIC_UNSHARE));
  }
}

void TabMenuModel::Build(int index) {
  std::vector<int> indices;
  if (tab_strip_->IsTabSelected(index)) {
    const ui::ListSelectionModel::SelectedIndices sel =
        tab_strip_->selection_model()
            .GetListSelectionModel()
            .selected_indices();
    indices = std::vector<int>(sel.begin(), sel.end());
  } else {
    indices = {index};
  }

  int num_tabs = indices.size();

  auto* controller = tabs::VerticalTabStripStateController::From(
      tab_strip_->delegate()->GetBrowserWindowInterface());
  bool showing_vertical_tabs =
      controller && controller->ShouldDisplayVerticalTabs();

  if (showing_vertical_tabs) {
    AddItemWithStringId(TabStripModel::CommandNewTabToRight,
                        IDS_TAB_CXMENU_NEWTABBELOW);
  } else {
    AddItemWithStringId(TabStripModel::CommandNewTabToRight,
                        base::i18n::IsRTL() ? IDS_TAB_CXMENU_NEWTABTOLEFT
                                            : IDS_TAB_CXMENU_NEWTABTORIGHT);
  }
  SetElementIdentifierAt(GetItemCount() - 1, kAddNewTabAdjacentMenuItem);

  if (!tab_strip_->GetSplitForTab(index).has_value()) {
    if (tab_strip_->GetActiveTab()->IsSplit()) {
      swap_with_split_submenu_ =
          std::make_unique<SplitTabSwapMenuModel>(tab_strip_, index);
      AddSubMenuWithStringIdAndIcon(
          TabStripModel::CommandSwapWithActiveSplit,
          IDS_TAB_CXMENU_SWAP_WITH_ACTIVE_SPLIT, swap_with_split_submenu_.get(),
          ui::ImageModel::FromVectorIcon(kSplitSceneIcon, ui::kColorMenuIcon,
                                         kTabMenuIconSize));
      const int swap_with_split_index = GetItemCount() - 1;
      SetEnabledAt(swap_with_split_index, num_tabs == 1);
      SetElementIdentifierAt(swap_with_split_index, kSwapSplitTabsMenuItem);
    } else {
      if (tabs::kSplitViewHorizontalDirectAccess.Get()) {
        split_orientation_submenu_ = std::make_unique<SplitViewLayoutMenuModel>(
            tab_strip_, tab_strip_->GetTabAtIndex(index)->GetHandle());
        AddSubMenuWithStringIdAndIcon(
            TabStripModel::CommandAddToSplit,
            index == tab_strip_->active_index()
                ? IDS_TAB_CXMENU_ADD_TAB_TO_NEW_SPLIT
                : IDS_TAB_CXMENU_NEW_SPLIT_WITH_CURRENT,
            split_orientation_submenu_.get(),
            ui::ImageModel::FromVectorIcon(kSplitSceneIcon, ui::kColorMenuIcon,
                                           kTabMenuIconSize));
      } else {
        AddItemWithStringIdAndIcon(
            TabStripModel::CommandAddToSplit,
            index == tab_strip_->active_index()
                ? IDS_TAB_CXMENU_ADD_TAB_TO_NEW_SPLIT
                : IDS_TAB_CXMENU_NEW_SPLIT_WITH_CURRENT,
            ui::ImageModel::FromVectorIcon(kSplitSceneIcon, ui::kColorMenuIcon,
                                           kTabMenuIconSize));
      }
      const int add_to_split_index = GetItemCount() - 1;
      SetEnabledAt(add_to_split_index, num_tabs == 1 || num_tabs == 2);
      SetElementIdentifierAt(add_to_split_index, kSplitTabsMenuItem);
    }
  } else {
    arrange_split_view_submenu_ = std::make_unique<SplitTabMenuModel>(
        tab_strip_, SplitTabMenuModel::MenuSource::kTabContextMenu, index);
    AddSubMenuWithStringIdAndIcon(
        TabStripModel::CommandArrangeSplit, IDS_TAB_CXMENU_ARRANGE_SPLIT,
        arrange_split_view_submenu_.get(),
        ui::ImageModel::FromVectorIcon(kSplitSceneIcon, ui::kColorMenuIcon,
                                       kTabMenuIconSize));
    SetElementIdentifierAt(GetItemCount() - 1, kArrangeSplitTabsMenuItem);
  }

  if (ExistingTabGroupSubMenuModel::ShouldShowSubmenu(
          tab_strip_, index, tab_menu_model_delegate_)) {
    // Create submenu with existing groups
    add_to_existing_group_submenu_ =
        std::make_unique<ExistingTabGroupSubMenuModel>(
            delegate(), tab_menu_model_delegate_, tab_strip_, index);
    AddSubMenu(TabStripModel::CommandAddToExistingGroup,
               l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_ADD_TAB_TO_GROUP,
                                                num_tabs),
               add_to_existing_group_submenu_.get());
    if (base::FeatureList::IsEnabled(features::kMenuSimplification)) {
      SetIconForCommandId(
          TabStripModel::CommandAddToExistingGroup,
          ui::ImageModel::FromVectorIcon(
              kSavedTabGroupBarEverythingIcon, ui::kColorMenuIcon,
              ui::SimpleMenuModel::kDefaultIconSize));
    }
  } else {
    AddItem(TabStripModel::CommandAddToNewGroup,
            l10n_util::GetPluralStringFUTF16(
                IDS_TAB_CXMENU_ADD_TAB_TO_NEW_GROUP, num_tabs));
    SetElementIdentifierAt(GetItemCount() - 1, kAddToNewGroupItemIdentifier);
    if (base::FeatureList::IsEnabled(features::kMenuSimplification)) {
      SetIconForCommandId(
          TabStripModel::CommandAddToNewGroup,
          ui::ImageModel::FromVectorIcon(
              kSavedTabGroupBarEverythingIcon, ui::kColorMenuIcon,
              ui::SimpleMenuModel::kDefaultIconSize));
    }
  }

  for (const auto& selection : indices) {
    if (tab_strip_->GetTabGroupForTab(selection).has_value()) {
      AddItemWithStringId(TabStripModel::CommandRemoveFromGroup,
                          IDS_TAB_CXMENU_REMOVE_TAB_FROM_GROUP);
      break;
    }
  }

  if (ExistingWindowSubMenuModel::ShouldShowSubmenu(tab_strip_->profile())) {
    // Create submenu with existing windows
    add_to_existing_window_submenu_ = ExistingWindowSubMenuModel::Create(
        delegate(), tab_menu_model_delegate_, tab_strip_, index);
    AddSubMenu(TabStripModel::CommandMoveToExistingWindow,
               l10n_util::GetPluralStringFUTF16(
                   IDS_TAB_CXMENU_MOVETOANOTHERWINDOW, num_tabs),
               add_to_existing_window_submenu_.get());
    if (base::FeatureList::IsEnabled(features::kMenuSimplification)) {
      SetIconForCommandId(TabStripModel::CommandMoveToExistingWindow,
                          ui::ImageModel::FromVectorIcon(
                              kOpenInNewIcon, ui::kColorMenuIcon,
                              ui::SimpleMenuModel::kDefaultIconSize));
    }
  } else {
    AddItem(TabStripModel::CommandMoveTabsToNewWindow,
            l10n_util::GetPluralStringFUTF16(
                IDS_TAB_CXMENU_MOVE_TABS_TO_NEW_WINDOW, num_tabs));
    if (base::FeatureList::IsEnabled(features::kMenuSimplification)) {
      SetIconForCommandId(TabStripModel::CommandMoveTabsToNewWindow,
                          ui::ImageModel::FromVectorIcon(
                              kOpenInNewIcon, ui::kColorMenuIcon,
                              ui::SimpleMenuModel::kDefaultIconSize));
    }
  }

  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringId(TabStripModel::CommandReload, IDS_TAB_CXMENU_RELOAD);

  AddItemWithStringId(TabStripModel::CommandDuplicate,
                      IDS_TAB_CXMENU_DUPLICATE);
  SetElementIdentifierAt(GetItemCount() - 1, kDuplicateMenuItem);

  bool will_pin = tab_strip_->WillContextMenuPin(index);
  AddItemWithStringId(
      TabStripModel::CommandTogglePinned,
      will_pin ? IDS_TAB_CXMENU_PIN_TAB : IDS_TAB_CXMENU_UNPIN_TAB);

  if (base::FeatureList::IsEnabled(features::kMenuSimplification)) {
    SetIconForCommandId(
        TabStripModel::CommandTogglePinned,
        ui::ImageModel::FromVectorIcon(
            will_pin ? views::kPinOldIcon : views::kUnpinOldIcon,
            ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize));
  }

  const bool will_mute = !AreAllSitesMuted(*tab_strip_, indices);
  AddItem(TabStripModel::CommandToggleSiteMuted,
          will_mute ? l10n_util::GetPluralStringFUTF16(
                          IDS_TAB_CXMENU_SOUND_MUTE_SITE, num_tabs)
                    : l10n_util::GetPluralStringFUTF16(
                          IDS_TAB_CXMENU_SOUND_UNMUTE_SITE, num_tabs));

  if (base::FeatureList::IsEnabled(features::kMenuSimplification)) {
    SetIconForCommandId(
        TabStripModel::CommandToggleSiteMuted,
        ui::ImageModel::FromVectorIcon(
            will_mute ? vector_icons::kVolumeOffChromeRefreshIcon
                      : vector_icons::kVolumeUpChromeRefreshIcon,
            ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize));
  }

  const bool show_glic_items =
      glic::GlicEnabling::IsReadyForProfile(tab_strip_->profile()) &&
      base::FeatureList::IsEnabled(features::kGlicMITabContextMenu);
  bool glic_displayed = false;
  if (base::FeatureList::IsEnabled(features::kMenuSimplification) &&
      show_glic_items) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AppendGlicItems(index, num_tabs, indices);
    AddSeparator(ui::NORMAL_SEPARATOR);
    glic_displayed = true;
  }

  const bool display_read_later = tab_strip_->delegate()->SupportsReadLater();
  const std::optional<send_tab_to_self::EntryPointDisplayReason>
      send_tab_to_self_reason = send_tab_to_self::GetEntryPointDisplayReason(
          tab_strip_->GetWebContentsAt(index));
  const bool display_send_to_self = send_tab_to_self_reason.has_value();

  if ((display_read_later || display_send_to_self) && !glic_displayed) {
    AddSeparator(ui::NORMAL_SEPARATOR);
  }

  if (display_read_later) {
    AddItemWithIcon(
        TabStripModel::CommandAddToReadLater,
        l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_READ_LATER, num_tabs),
        ui::ImageModel::FromVectorIcon(kMenuBookChromeRefreshIcon,
                                       ui::kColorMenuIcon, kTabMenuIconSize));
    SetEnabledAt(GetItemCount() - 1,
                 tab_strip_->IsReadLaterSupportedForAny(indices));
  }

  if (show_glic_items && !glic_displayed) {
    AppendGlicItems(index, num_tabs, indices);
  }

  if (display_send_to_self) {
    if (base::FeatureList::IsEnabled(
            send_tab_to_self::kSendTabToSelfEnhancedDesktopUI) &&
        send_tab_to_self_reason ==
            send_tab_to_self::EntryPointDisplayReason::kOfferFeature) {
      BuildSendTabToSelfSubmenu(index);
    } else {
      if (send_tab_to_self_reason !=
          send_tab_to_self::EntryPointDisplayReason::kOfferFeature) {
        // TODO(crbug.com/488252159): Add edge cases (e.g. not signed in
        // or no target devices available) when UI is fully specified.
      }
      BuildLegacySendTabToSelfItem();
    }
  }

  if (tabs::kVerticalTabsToggleInTabContextMenu.Get() && controller) {
    // TODO(crbug.com/475222200): When in immersive, swapping between tab
    // strip types create duplicate tab strips. Until that is resolved,
    // disable the ability to swap between tab strips while in immersive.
    BrowserWindowInterface* bwi =
        tab_strip_->delegate()->GetBrowserWindowInterface();
    if (bwi && !bwi->GetFeatures().immersive_mode_controller()->IsEnabled()) {
      AddSeparator(ui::NORMAL_SEPARATOR);
      if (controller->ShouldDisplayVerticalTabs()) {
        AddItemWithStringId(TabStripModel::CommandToggleVertical,
                            IDS_SWITCH_TO_HORIZONTAL_TAB);
      } else {
        AddItemWithStringId(TabStripModel::CommandToggleVertical,
                            IDS_SWITCH_TO_VERTICAL_TAB);
        const bool use_preview_badge =
            base::FeatureList::IsEnabled(tabs::kVerticalTabsPreviewBadge);
        const user_education::DisplayNewBadge show_badge =
            UserEducationService::MaybeShowNewBadge(
                tab_strip_->profile(), use_preview_badge
                                           ? tabs::kVerticalTabsPreviewBadge
                                           : tabs::kVerticalTabsNewBadge);
        SetIsNewFeatureAt(GetItemCount() - 1, show_badge);
      }
    }
  }

// Append extension items for the 'tab' context if the feature is enabled.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionTabContextMenu)) {
    extension_items_ = std::make_unique<extensions::ContextMenuMatcher>(
        tab_strip_->profile(), delegate(), this,
        base::BindRepeating([](const extensions::MenuItem* item) {
          return item->contexts().Contains(extensions::MenuItem::TAB);
        }));

    int extension_index = 0;
    // Iterate over all extensions that have menu items loaded in the
    // MenuManager.
    for (const auto& key :
         extensions::MenuManager::Get(tab_strip_->profile())->ExtensionIds()) {
      extension_items_->AppendExtensionItems(key, std::u16string(),
                                             &extension_index,
                                             /*is_action_menu=*/false);
    }
  }
#endif

  // Separator Close Tab items
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringId(TabStripModel::CommandCloseTab, IDS_TAB_CXMENU_CLOSETAB);
  AddItemWithStringId(TabStripModel::CommandCloseOtherTabs,
                      IDS_TAB_CXMENU_CLOSEOTHERTABS);

  if (showing_vertical_tabs) {
    AddItemWithStringId(TabStripModel::CommandCloseTabsToRight,
                        IDS_TAB_CXMENU_CLOSETABSBELOW);
  } else {
    AddItemWithStringId(TabStripModel::CommandCloseTabsToRight,
                        base::i18n::IsRTL() ? IDS_TAB_CXMENU_CLOSETABSTOLEFT
                                            : IDS_TAB_CXMENU_CLOSETABSTORIGHT);
  }
  SetEnabledAt(GetItemCount() - 1,
               tab_strip_->IsContextMenuCommandEnabled(
                   index, TabStripModel::CommandCloseTabsToRight));
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabMenuModel,
                                      kAddToNewGroupItemIdentifier);

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Helper to retrieve the extension matcher if item at 'index' is an extension.
extensions::ContextMenuMatcher* TabMenuModel::GetMatcherIfExtension(
    size_t index) const {
  int command_id = GetCommandIdAt(index);
  if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(command_id)) {
    return extension_items_.get();
  }
  return nullptr;
}
#endif

// Overridden to check if the checked status of the item
// should be evaluated by the extension matcher instead of the simple model.
bool TabMenuModel::IsItemCheckedAt(size_t index) const {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (auto* matcher = GetMatcherIfExtension(index)) {
    return matcher->IsCommandIdChecked(GetCommandIdAt(index));
  }
#endif
  return ui::SimpleMenuModel::IsItemCheckedAt(index);
}

// Overridden to check if the enabled status of the item
// should be evaluated by the extension matcher instead of the simple model.
bool TabMenuModel::IsEnabledAt(size_t index) const {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (auto* matcher = GetMatcherIfExtension(index)) {
    return matcher->IsCommandIdEnabled(GetCommandIdAt(index));
  }
#endif
  return ui::SimpleMenuModel::IsEnabledAt(index);
}

// Overridden to check if the visible status of the item
// should be evaluated by the extension matcher instead of the simple model.
bool TabMenuModel::IsVisibleAt(size_t index) const {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (auto* matcher = GetMatcherIfExtension(index)) {
    return matcher->IsCommandIdVisible(GetCommandIdAt(index));
  }
#endif
  return ui::SimpleMenuModel::IsVisibleAt(index);
}

void TabMenuModel::ActivatedAt(size_t index) {
  ActivatedAt(index, 0);
}

// Overridden to execute core context menu command logic on activation for
// extension items.
void TabMenuModel::ActivatedAt(size_t index, int event_flags) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (auto* matcher = GetMatcherIfExtension(index)) {
    content::WebContents* web_contents =
        tab_interface_ ? tab_interface_->GetContents() : nullptr;
    // The underlying tab could be closed, crashed, or moved in the background
    // while the context menu is still open.
    if (web_contents) {
      // Create minimal ContextMenuParams sufficient for executing the command
      // for 'contextMenus' extension API. E.g. providing the current page
      // URL.
      content::ContextMenuParams params;
      params.page_url = web_contents->GetLastCommittedURL();
      matcher->ExecuteCommand(GetCommandIdAt(index), web_contents, nullptr,
                              params);
    }
    return;
  }
#endif
  ui::SimpleMenuModel::ActivatedAt(index, event_flags);
}
