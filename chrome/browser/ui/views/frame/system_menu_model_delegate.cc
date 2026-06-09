// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/system_menu_model_delegate.h"

#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_metrics.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ui/frame/desks/move_to_desks_menu_delegate.h"
#include "chromeos/ui/frame/desks/move_to_desks_menu_model.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "chrome/common/pref_names.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif  // BUILDFLAG(IS_WIN)

SystemMenuModelDelegate::SystemMenuModelDelegate(
    ui::AcceleratorProvider* provider,
    Browser* browser)
    : provider_(provider), browser_(browser) {}

SystemMenuModelDelegate::~SystemMenuModelDelegate() = default;

bool SystemMenuModelDelegate::IsCommandIdChecked(int command_id) const {
#if BUILDFLAG(IS_LINUX)
  if (command_id == IDC_USE_SYSTEM_TITLE_BAR) {
    PrefService* prefs = browser_->profile()->GetPrefs();
    return !prefs->GetBoolean(prefs::kUseCustomChromeFrame);
  }
#endif
  return false;
}

bool SystemMenuModelDelegate::IsCommandIdEnabled(int command_id) const {
#if BUILDFLAG(IS_CHROMEOS)
  if (command_id == chromeos::MoveToDesksMenuModel::kMenuCommandId) {
    return chromeos::MoveToDesksMenuDelegate::ShouldShowMoveToDesksMenu();
  }
#endif
  if (command_id == IDC_TAB_SEARCH_TOGGLE_PIN) {
    return base::FeatureList::IsEnabled(tabs::kHorizontalTabStripComboButton);
  }
  // Disable the glic toggle pin if it is showing and glic is not enabled.
  if (command_id == IDC_GLIC_TOGGLE_PIN) {
    return glic::GlicEnabling::IsEnabledForProfile(browser_->profile());
  }

#if BUILDFLAG(IS_WIN)
  if (features::IsMenuSimplificationEnabled()) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
    if (browser_view) {
      switch (command_id) {
        case IDC_RESTORE_WINDOW:
          return browser_view->IsMaximized() || browser_view->IsMinimized();
        case IDC_MOVE_WINDOW:
          return !browser_view->IsMaximized();
        case IDC_SIZE_WINDOW:
          return !browser_view->IsMaximized() && browser_view->CanResize();
        case IDC_MINIMIZE_WINDOW:
          return browser_view->CanMinimize();
        case IDC_MAXIMIZE_WINDOW:
          return !browser_view->IsMaximized() && browser_view->CanMaximize();
      }
    }
  }
#endif  // BUILDFLAG(IS_WIN)

  return chrome::IsCommandEnabled(browser_, command_id);
}

bool SystemMenuModelDelegate::IsCommandIdVisible(int command_id) const {
#if BUILDFLAG(IS_LINUX)
  bool is_maximized = browser_->GetWindow()->IsMaximized();
  switch (command_id) {
    case IDC_MAXIMIZE_WINDOW:
      return !is_maximized;
    case IDC_RESTORE_WINDOW:
      return is_maximized;
  }
#endif
#if BUILDFLAG(IS_CHROMEOS)
  if (command_id == chromeos::MoveToDesksMenuModel::kMenuCommandId) {
    return chromeos::MoveToDesksMenuDelegate::ShouldShowMoveToDesksMenu();
  }
#endif
  if (command_id == IDC_TAB_SEARCH_TOGGLE_PIN) {
    return base::FeatureList::IsEnabled(tabs::kHorizontalTabStripComboButton);
  }
  if (command_id == IDC_GLIC_TOGGLE_PIN) {
    return glic::GlicEnabling::IsEnabledForProfile(browser_->profile());
  }
  return true;
}

bool SystemMenuModelDelegate::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return provider_->GetAcceleratorForCommandId(command_id, accelerator);
}

bool SystemMenuModelDelegate::IsItemForCommandIdDynamic(int command_id) const {
  return std::set{IDC_RESTORE_TAB, IDC_TAB_SEARCH_TOGGLE_PIN,
                  IDC_GLIC_TOGGLE_PIN, IDC_TOGGLE_VERTICAL_TABS,
                  IDC_TOGGLE_VERTICAL_TABS_EXPAND_ON_HOVER}
      .contains(command_id);
}

std::u16string SystemMenuModelDelegate::GetLabelForCommandId(
    int command_id) const {
  DCHECK(IsItemForCommandIdDynamic(command_id));

  int string_id;
  switch (command_id) {
    case IDC_RESTORE_TAB:
      string_id = IDS_RESTORE_TAB;
      if (IsCommandIdEnabled(command_id)) {
        sessions::TabRestoreService* trs =
            TabRestoreServiceFactory::GetForProfile(browser_->profile());
        DCHECK(trs);
        trs->LoadTabsFromLastSession();
        if (!trs->entries().empty()) {
          switch (trs->entries().front()->type) {
            case sessions::tab_restore::Type::WINDOW:
              string_id = IDS_REOPEN_WINDOW;
              break;
            case sessions::tab_restore::Type::GROUP:
              string_id = IDS_REOPEN_GROUP;
              break;
            case sessions::tab_restore::Type::SPLIT:
              string_id = IDS_REOPEN_SPLIT;
              break;
            case sessions::tab_restore::Type::TAB:
              break;
          }
        }
      }
      break;
    case IDC_TOGGLE_VERTICAL_TABS: {
      auto* controller = tabs::VerticalTabStripStateController::From(browser_);
      CHECK(controller);
      string_id = controller->ShouldDisplayVerticalTabs()
                      ? IDS_SWITCH_TO_HORIZONTAL_TAB
                      : IDS_SWITCH_TO_VERTICAL_TAB;
      break;
    }
    case IDC_TOGGLE_VERTICAL_TABS_EXPAND_ON_HOVER: {
      auto* controller = tabs::VerticalTabStripStateController::From(browser_);
      CHECK(controller);
      string_id = controller->IsExpandOnHoverEnabled()
                      ? IDS_VERTICAL_TABS_DISABLE_EXPAND_ON_HOVER
                      : IDS_VERTICAL_TABS_ENABLE_EXPAND_ON_HOVER;
      break;
    }
    case IDC_TAB_SEARCH_TOGGLE_PIN:
      string_id = browser_->profile()->GetPrefs()->GetBoolean(
                      prefs::kTabSearchPinnedToTabstrip)
                      ? IDS_TAB_STRIP_UNPIN_TAB_SEARCH
                      : IDS_TAB_STRIP_PIN_TAB_SEARCH;
      break;
    case IDC_GLIC_TOGGLE_PIN:
      string_id = browser_->profile()->GetPrefs()->GetBoolean(
                      glic::prefs::kGlicPinnedToTabstrip)
                      ? IDS_GLIC_UNPIN
                      : IDS_GLIC_PIN;
      break;
    default:
      NOTREACHED();
  }
  return l10n_util::GetStringUTF16(string_id);
}

void SystemMenuModelDelegate::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case IDC_BOOKMARK_ALL_TABS:
      base::RecordAction(
          base::UserMetricsAction("SystemContextMenu_BookmarkAllTabs"));
      break;
    case IDC_NEW_TAB:
      base::RecordAction(base::UserMetricsAction("SystemContextMenu_NewTab"));
      break;
    case IDC_RESTORE_TAB:
      base::RecordAction(
          base::UserMetricsAction("SystemContextMenu_RestoreTab"));
      break;
    case IDC_GROUP_UNGROUPED_TABS:
      base::RecordAction(
          base::UserMetricsAction("SystemContextMenu_GroupAllTabs"));
      break;
    case IDC_NAME_WINDOW:
      base::RecordAction(
          base::UserMetricsAction("SystemContextMenu_NameWindow"));
      break;
    case IDC_TOGGLE_VERTICAL_TABS: {
      auto* controller = tabs::VerticalTabStripStateController::From(browser_);
      if (controller) {
        const bool is_vertical = !controller->ShouldDisplayVerticalTabs();
        tabs::RecordVerticalTabStripModeChanged(
            is_vertical, tabs::VerticalTabStripEntryPoint::kSystemContextMenu);
      }
      break;
    }
    case IDC_TAB_SEARCH_TOGGLE_PIN: {
      if (base::FeatureList::IsEnabled(tabs::kHorizontalTabStripComboButton)) {
        PrefService* prefs = browser_->profile()->GetPrefs();
        const bool is_pinned =
            prefs->GetBoolean(prefs::kTabSearchPinnedToTabstrip);
        base::RecordAction(base::UserMetricsAction(
            is_pinned ? "SystemContextMenu_TabSearch_Unpinned"
                      : "SystemContextMenu_TabSearch_Pinned"));
      }
      break;
    }
  }
  chrome::ExecuteCommand(browser_, command_id);
}

void SystemMenuModelDelegate::OnMenuWillShow(ui::SimpleMenuModel* source) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  CHECK(browser_view);
  CHECK(browser_view->tab_strip_view());
  expand_on_hover_lock_ = browser_view->tab_strip_view()->GetExpandOnHoverLock(
      ExpandOnHoverLockType::kKeepCurrentState);
}

void SystemMenuModelDelegate::MenuClosed(ui::SimpleMenuModel* source) {
  expand_on_hover_lock_.reset();
}
