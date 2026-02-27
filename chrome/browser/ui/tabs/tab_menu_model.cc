// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_menu_model.h"

#include <memory>

#include "base/command_line.h"
#include "base/feature_list.h"
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
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/existing_tab_group_sub_menu_model.h"
#include "chrome/browser/ui/tabs/existing_window_sub_menu_model.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/glic_tab_sub_menu_model.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/tabs/split_tab_menu_model.h"
#include "chrome/browser/ui/tabs/split_tab_swap_menu_model.h"
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
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/feed/feed_feature_list.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

using base::UserMetricsAction;

namespace {
constexpr int kTabMenuIconSize = 16;
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabMenuModel, kAddANoteTabMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabMenuModel, kSplitTabsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabMenuModel, kArrangeSplitTabsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabMenuModel, kSwapSplitTabsMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabMenuModel, kAddNewTabAdjacentMenuItem);

TabMenuModel::TabMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                           TabMenuModelDelegate* tab_menu_model_delegate,
                           TabStripModel* tab_strip,
                           int index)
    : ui::SimpleMenuModel(delegate),
      tab_menu_model_delegate_(tab_menu_model_delegate) {
  if (tab_strip->delegate()->IsForWebApp()) {
    BuildForWebApp(tab_strip, index);
  } else {
    Build(tab_strip, index);
  }
}

TabMenuModel::~TabMenuModel() = default;

void TabMenuModel::BuildForWebApp(TabStripModel* tab_strip, int index) {
  AddItemWithStringId(TabStripModel::CommandCopyURL, IDS_COPY_URL);
  AddItemWithStringId(TabStripModel::CommandReload, IDS_TAB_CXMENU_RELOAD);
  AddItemWithStringId(TabStripModel::CommandGoBack, IDS_CONTENT_CONTEXT_BACK);

  if (!web_app::IsPinnedHomeTab(tab_strip, index) &&
      (!web_app::HasPinnedHomeTab(tab_strip) ||
       !tab_strip->selection_model().IsSelected(*tab_strip->begin()))) {
    int num_tabs = tab_strip->selection_model().size();
    if (ExistingWindowSubMenuModel::ShouldShowSubmenuForApp(
            tab_menu_model_delegate_)) {
      // Create submenu with existing windows
      add_to_existing_window_submenu_ = ExistingWindowSubMenuModel::Create(
          delegate(), tab_menu_model_delegate_, tab_strip, index);
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

  if (!web_app::IsPinnedHomeTab(tab_strip, index)) {
    AddItemWithStringId(TabStripModel::CommandCloseTab,
                        IDS_TAB_CXMENU_CLOSETAB);
    AddItemWithStringId(TabStripModel::CommandCloseOtherTabs,
                        IDS_TAB_CXMENU_CLOSEOTHERTABS);
  }
  if (web_app::HasPinnedHomeTab(tab_strip)) {
    AddItemWithStringId(TabStripModel::CommandCloseAllTabs,
                        IDS_TAB_CXMENU_CLOSEALLTABS);
  }
}

void TabMenuModel::Build(TabStripModel* tab_strip, int index) {
  std::vector<int> indices;
  if (tab_strip->IsTabSelected(index)) {
    const ui::ListSelectionModel::SelectedIndices sel =
        tab_strip->selection_model().GetListSelectionModel().selected_indices();
    indices = std::vector<int>(sel.begin(), sel.end());
  } else {
    indices = {index};
  }

  int num_tabs = indices.size();

  auto* controller = tabs::VerticalTabStripStateController::From(
      tab_strip->delegate()->GetBrowserWindowInterface());
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

  if (!tab_strip->GetSplitForTab(index).has_value()) {
    if (tab_strip->GetActiveTab()->IsSplit()) {
      swap_with_split_submenu_ =
          std::make_unique<SplitTabSwapMenuModel>(tab_strip, index);
      AddSubMenuWithStringIdAndIcon(
          TabStripModel::CommandSwapWithActiveSplit,
          IDS_TAB_CXMENU_SWAP_WITH_ACTIVE_SPLIT, swap_with_split_submenu_.get(),
          ui::ImageModel::FromVectorIcon(kSplitSceneIcon, ui::kColorMenuIcon,
                                         kTabMenuIconSize));
      const int swap_with_split_index = GetItemCount() - 1;
      SetEnabledAt(swap_with_split_index, num_tabs == 1);
      SetElementIdentifierAt(swap_with_split_index, kSwapSplitTabsMenuItem);
    } else {
      AddItemWithStringIdAndIcon(
          TabStripModel::CommandAddToSplit,
          index == tab_strip->active_index()
              ? IDS_TAB_CXMENU_ADD_TAB_TO_NEW_SPLIT
              : IDS_TAB_CXMENU_NEW_SPLIT_WITH_CURRENT,
          ui::ImageModel::FromVectorIcon(kSplitSceneIcon, ui::kColorMenuIcon,
                                         kTabMenuIconSize));
      const int add_to_split_index = GetItemCount() - 1;
      SetEnabledAt(add_to_split_index, num_tabs == 1 || num_tabs == 2);
      SetElementIdentifierAt(add_to_split_index, kSplitTabsMenuItem);
    }
  } else {
    arrange_split_view_submenu_ = std::make_unique<SplitTabMenuModel>(
        tab_strip, SplitTabMenuModel::MenuSource::kTabContextMenu, index);
    AddSubMenuWithStringIdAndIcon(
        TabStripModel::CommandArrangeSplit, IDS_TAB_CXMENU_ARRANGE_SPLIT,
        arrange_split_view_submenu_.get(),
        ui::ImageModel::FromVectorIcon(kSplitSceneIcon, ui::kColorMenuIcon,
                                       kTabMenuIconSize));
    SetElementIdentifierAt(GetItemCount() - 1, kArrangeSplitTabsMenuItem);
  }
  SetIsNewFeatureAt(GetItemCount() - 1,
                    UserEducationService::MaybeShowNewBadge(
                        tab_strip->profile(), features::kSideBySide));

  if (ExistingTabGroupSubMenuModel::ShouldShowSubmenu(
          tab_strip, index, tab_menu_model_delegate_)) {
    // Create submenu with existing groups
    add_to_existing_group_submenu_ =
        std::make_unique<ExistingTabGroupSubMenuModel>(
            delegate(), tab_menu_model_delegate_, tab_strip, index);
    AddSubMenu(TabStripModel::CommandAddToExistingGroup,
               l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_ADD_TAB_TO_GROUP,
                                                num_tabs),
               add_to_existing_group_submenu_.get());
  } else {
    AddItem(TabStripModel::CommandAddToNewGroup,
            l10n_util::GetPluralStringFUTF16(
                IDS_TAB_CXMENU_ADD_TAB_TO_NEW_GROUP, num_tabs));
    SetElementIdentifierAt(GetItemCount() - 1, kAddToNewGroupItemIdentifier);
  }

  for (const auto& selection : indices) {
    if (tab_strip->GetTabGroupForTab(selection).has_value()) {
      AddItemWithStringId(TabStripModel::CommandRemoveFromGroup,
                          IDS_TAB_CXMENU_REMOVE_TAB_FROM_GROUP);
      break;
    }
  }

  if (ExistingWindowSubMenuModel::ShouldShowSubmenu(tab_strip->profile())) {
    // Create submenu with existing windows
    add_to_existing_window_submenu_ = ExistingWindowSubMenuModel::Create(
        delegate(), tab_menu_model_delegate_, tab_strip, index);
    AddSubMenu(TabStripModel::CommandMoveToExistingWindow,
               l10n_util::GetPluralStringFUTF16(
                   IDS_TAB_CXMENU_MOVETOANOTHERWINDOW, num_tabs),
               add_to_existing_window_submenu_.get());
  } else {
    AddItem(TabStripModel::CommandMoveTabsToNewWindow,
            l10n_util::GetPluralStringFUTF16(
                IDS_TAB_CXMENU_MOVE_TABS_TO_NEW_WINDOW, num_tabs));
  }

  if (TabOrganizationUtils::GetInstance()->IsEnabled(tab_strip->profile())) {
    auto* const tab_organization_service =
        TabOrganizationServiceFactory::GetForProfile(tab_strip->profile());
    if (tab_organization_service) {
      AddItemWithStringId(TabStripModel::CommandOrganizeTabs,
                          IDS_TAB_CXMENU_ORGANIZE_TABS);
    }
  }

  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringId(TabStripModel::CommandReload, IDS_TAB_CXMENU_RELOAD);

  AddItemWithStringId(TabStripModel::CommandDuplicate,
                      IDS_TAB_CXMENU_DUPLICATE);

  bool will_pin = tab_strip->WillContextMenuPin(index);
  AddItemWithStringId(
      TabStripModel::CommandTogglePinned,
      will_pin ? IDS_TAB_CXMENU_PIN_TAB : IDS_TAB_CXMENU_UNPIN_TAB);

  const bool will_mute = !AreAllSitesMuted(*tab_strip, indices);
  AddItem(TabStripModel::CommandToggleSiteMuted,
          will_mute ? l10n_util::GetPluralStringFUTF16(
                          IDS_TAB_CXMENU_SOUND_MUTE_SITE, num_tabs)
                    : l10n_util::GetPluralStringFUTF16(
                          IDS_TAB_CXMENU_SOUND_UNMUTE_SITE, num_tabs));

  const bool display_read_later = tab_strip->delegate()->SupportsReadLater();
  const bool display_send_to_self = send_tab_to_self::ShouldDisplayEntryPoint(
      tab_strip->GetWebContentsAt(index));
  const bool display_share_with_glic =
      base::FeatureList::IsEnabled(glic::mojom::features::kGlicMultiTab) &&
      glic::GlicEnabling::IsReadyForProfile(tab_strip->profile()) &&
      !glic::GlicEnabling::IsMultiInstanceEnabled();
  if (display_read_later || display_send_to_self || display_share_with_glic) {
    AddSeparator(ui::NORMAL_SEPARATOR);
  }

  if (display_read_later) {
    AddItemWithIcon(
        TabStripModel::CommandAddToReadLater,
        l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_READ_LATER, num_tabs),
        ui::ImageModel::FromVectorIcon(kMenuBookChromeRefreshIcon,
                                       ui::kColorMenuIcon, kTabMenuIconSize));
    SetEnabledAt(GetItemCount() - 1,
                 tab_strip->IsReadLaterSupportedForAny(indices));
  }

  if (glic::GlicEnabling::IsReadyForProfile(tab_strip->profile()) &&
      glic::GlicEnabling::IsMultiInstanceEnabled() &&
      base::FeatureList::IsEnabled(features::kGlicMITabContextMenu)) {
    glic_tab_sub_menu_model_ =
        std::make_unique<glic::GlicTabSubMenuModel>(tab_strip, index);
    AddSubMenu(TabStripModel::CommandGlicShare,
               l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_GLIC_START_SHARE,
                                                num_tabs),
               glic_tab_sub_menu_model_.get());

    auto* service = glic::GlicKeyedServiceFactory::GetGlicKeyedService(
        tab_strip->profile());
    CHECK(service);
    if (std::ranges::any_of(indices, [&](int index) {
          return service->IsTabPinnedToAnyInstance(
              tab_strip->GetTabAtIndex(index)->GetHandle());
        })) {
      AddItem(TabStripModel::CommandGlicUnshare,
              l10n_util::GetStringUTF16(IDS_TAB_CXMENU_GLIC_UNSHARE));
    }
  } else if (display_share_with_glic) {
    auto* service = glic::GlicKeyedServiceFactory::GetGlicKeyedService(
        tab_strip->profile());
    bool start_sharing = false;
    for (const auto& selection : indices) {
      if (!service->sharing_manager().IsTabPinned(
              tab_strip->GetTabAtIndex(selection)->GetHandle())) {
        start_sharing = true;
        break;
      }
    }
    if (start_sharing) {
      int32_t potential_count = service->sharing_manager().GetNumPinnedTabs() +
                                static_cast<int32_t>(indices.size());
      if (potential_count > service->sharing_manager().GetMaxPinnedTabs()) {
        AddItem(TabStripModel::CommandGlicShareLimit,
                l10n_util::GetPluralStringFUTF16(
                    IDS_TAB_CXMENU_GLIC_SHARE_LIMIT,
                    service->sharing_manager().GetMaxPinnedTabs()));
      } else {
        const gfx::VectorIcon& icon =
            glic::GlicVectorIconManager::GetVectorIcon(IDR_GLIC_ACCESSING_ICON);
        AddItemWithIcon(TabStripModel::CommandGlicStartShare,
                        l10n_util::GetPluralStringFUTF16(
                            IDS_TAB_CXMENU_GLIC_START_SHARE, num_tabs),
                        ui::ImageModel::FromVectorIcon(
                            icon, kColorTabAlertPipPlayingActiveFrameActive));
      }
    } else {
      AddItem(TabStripModel::CommandGlicStopShare,
              l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_GLIC_STOP_SHARE,
                                               num_tabs));
    }
  }

  if (display_send_to_self) {
#if BUILDFLAG(IS_MAC)
    AddItem(TabStripModel::CommandSendTabToSelf,
            l10n_util::GetStringUTF16(IDS_MENU_SEND_TAB_TO_SELF));
#else
    AddItemWithIcon(TabStripModel::CommandSendTabToSelf,
                    l10n_util::GetStringUTF16(IDS_MENU_SEND_TAB_TO_SELF),
                    ui::ImageModel::FromVectorIcon(kDevicesIcon));
#endif
  }

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
               tab_strip->IsContextMenuCommandEnabled(
                   index, TabStripModel::CommandCloseTabsToRight));

  if (controller) {
    // TODO(crbug.com/475222200): When in immersive, swapping between tab
    // strip types create duplicate tab strips. Until that is resolved,
    // disable the ability to swap between tab strips while in immersive.
    BrowserWindowInterface* bwi =
        tab_strip->delegate()->GetBrowserWindowInterface();
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
                tab_strip->profile(), use_preview_badge
                                          ? tabs::kVerticalTabsPreviewBadge
                                          : tabs::kVerticalTabsNewBadge);
        SetIsNewFeatureAt(GetItemCount() - 1, show_badge);
      }
    }
  }
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TabMenuModel,
                                      kAddToNewGroupItemIdentifier);
