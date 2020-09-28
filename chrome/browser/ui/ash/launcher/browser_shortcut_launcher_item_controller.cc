// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/browser_shortcut_launcher_item_controller.h"

#include <limits>
#include <utility>
#include <vector>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/wm/desks/desks_util.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/launcher/shelf_context_menu.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_prefs.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/image/image.h"

namespace {

// The tab-index flag for browser window menu items that do not specify a tab.
constexpr int kNoTab = std::numeric_limits<int>::max();

// Returns true when the given |browser| is listed in the browser application
// list.
bool IsBrowserRepresentedInBrowserList(Browser* browser) {
  // Only Ash desktop browser windows for the active user are represented.
  if (!browser || !multi_user_util::IsProfileFromActiveUser(browser->profile()))
    return false;

  if (browser->deprecated_is_app()) {
    // Crostini Terminals always have their own item.
    // TODO(rjwright): We shouldn't need to special-case Crostini here.
    // https://crbug.com/846546
    if (crostini::CrostiniAppIdFromAppName(browser->app_name()))
      return false;

    // V1 App popup windows may have their own item.
    ash::ShelfID id(web_app::GetAppIdFromApplicationName(browser->app_name()));
    if (ChromeLauncherController::instance()->GetItem(id))
      return false;
  }

  return true;
}

// Gets a list of active browsers.
BrowserList::BrowserVector GetListOfActiveBrowsers() {
  BrowserList::BrowserVector active_browsers;
  for (auto* browser : *BrowserList::GetInstance()) {
    // Only include browsers for the active user.
    if (!multi_user_util::IsProfileFromActiveUser(browser->profile()))
      continue;

    // Exclude invisible non-minimized browser windows on the active desk.
    aura::Window* native_window = browser->window()->GetNativeWindow();
    if (!browser->window()->IsVisible() && !browser->window()->IsMinimized() &&
        ash::desks_util::BelongsToActiveDesk(native_window)) {
      continue;
    }
    if (!IsBrowserRepresentedInBrowserList(browser) &&
        !browser->is_type_normal()) {
      continue;
    }
    active_browsers.push_back(browser);
  }
  return active_browsers;
}

bool ShouldRecordLaunchTime(Browser* browser) {
  return !browser->profile()->IsOffTheRecord() &&
         IsBrowserRepresentedInBrowserList(browser);
}

}  // namespace

BrowserShortcutLauncherItemController::BrowserShortcutLauncherItemController(
    ash::ShelfModel* shelf_model)
    : ash::ShelfItemDelegate(ash::ShelfID(extension_misc::kChromeAppId)),
      shelf_model_(shelf_model) {
  BrowserList::AddObserver(this);
  // Tag all open browser windows with the appropriate shelf id property. This
  // associates each window with the shelf item for the active web contents.
  for (auto* browser : *BrowserList::GetInstance()) {
    if (IsBrowserRepresentedInBrowserList(browser) &&
        browser->tab_strip_model()->GetActiveWebContents()) {
      SetShelfIDForBrowserWindowContents(
          browser, browser->tab_strip_model()->GetActiveWebContents());
    }
  }
}

BrowserShortcutLauncherItemController::
    ~BrowserShortcutLauncherItemController() {
  BrowserList::RemoveObserver(this);
}

void BrowserShortcutLauncherItemController::UpdateBrowserItemState() {
  // Determine the new browser's active state and change if necessary.
  int browser_index =
      shelf_model_->GetItemIndexForType(ash::TYPE_BROWSER_SHORTCUT);
  DCHECK_GE(browser_index, 0);
  ash::ShelfItem browser_item = shelf_model_->items()[browser_index];
  ash::ShelfItemStatus browser_status = ash::STATUS_CLOSED;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (IsBrowserRepresentedInBrowserList(browser)) {
      browser_status = ash::STATUS_RUNNING;
      break;
    }
  }

  if (browser_status != browser_item.status) {
    browser_item.status = browser_status;
    shelf_model_->Set(browser_index, browser_item);
  }
}

void BrowserShortcutLauncherItemController::SetShelfIDForBrowserWindowContents(
    Browser* browser,
    content::WebContents* web_contents) {
  // We need to set the window ShelfID for V1 applications since they are
  // content which might change and as such change the application type.
  // The browser window may not exist in unit tests.
  if (!browser || !browser->window() || !browser->window()->GetNativeWindow() ||
      !multi_user_util::IsProfileFromActiveUser(browser->profile())) {
    return;
  }

  const std::string app_id =
      ChromeLauncherController::instance()->GetAppIDForWebContents(
          web_contents);
  browser->window()->GetNativeWindow()->SetProperty(ash::kAppIDKey,
                                                    new std::string(app_id));

  const ash::ShelfID shelf_id =
      ChromeLauncherController::instance()->GetShelfIDForAppId(app_id);
  browser->window()->GetNativeWindow()->SetProperty(
      ash::kShelfIDKey, new std::string(shelf_id.Serialize()));
}

void BrowserShortcutLauncherItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback,
    const ItemFilterPredicate& filter_predicate) {
  if (event && (event->flags() & ui::EF_CONTROL_DOWN)) {
    ash::NewWindowDelegate::GetInstance()->NewWindow(/*incognito=*/false);
    std::move(callback).Run(ash::SHELF_ACTION_NEW_WINDOW_CREATED, {});
    return;
  }

  auto items =
      GetAppMenuItems(event ? event->flags() : ui::EF_NONE, filter_predicate);

  // In case of a keyboard event, we were called by a hotkey. In that case we
  // activate the next item in line if an item of our list is already active.
  if (event && event->type() == ui::ET_KEY_RELEASED) {
    std::move(callback).Run(ActivateOrAdvanceToNextBrowser(), std::move(items));
    return;
  }

  Profile* profile = ChromeLauncherController::instance()->profile();
  Browser* last_browser = chrome::FindTabbedBrowser(profile, true);

  if (last_browser && !filter_predicate.is_null() &&
      !filter_predicate.Run(last_browser->window()->GetNativeWindow())) {
    last_browser = nullptr;
  }

  if (!last_browser) {
    ash::NewWindowDelegate::GetInstance()->NewWindow(/*incognito=*/false);
    std::move(callback).Run(ash::SHELF_ACTION_NEW_WINDOW_CREATED, {});
    return;
  }

  ash::ShelfAction action;
  if (items.size() == 1) {
    // Single browser, activate or minimize if active.
    action =
        ChromeLauncherController::instance()->ActivateWindowOrMinimizeIfActive(
            last_browser->window(), true /* minimize allowed */);
  } else if (source == ash::LAUNCH_FROM_SHELF) {
    // Multiple targets, activating from shelf, a menu will be shown.
    // No need to activate or minimize the recently active browser.
    action = ash::SHELF_ACTION_NONE;
  } else {
    // Multiple targets, not activating from shelf, no menu will be shown.
    // Activate the recently active browser, never minimize.
    action =
        ChromeLauncherController::instance()->ActivateWindowOrMinimizeIfActive(
            last_browser->window(), false /* minimize not allowed */);
  }
  std::move(callback).Run(action, std::move(items));
}

ash::ShelfItemDelegate::AppMenuItems
BrowserShortcutLauncherItemController::GetAppMenuItems(
    int event_flags,
    const ItemFilterPredicate& filter_predicate) {
  std::vector<std::pair<Browser*, size_t>> app_menu_items;
  AppMenuItems items;
  bool found_tabbed_browser = false;
  ChromeLauncherController* controller = ChromeLauncherController::instance();
  for (auto* browser : GetListOfActiveBrowsers()) {
    if (!filter_predicate.is_null() &&
        !filter_predicate.Run(browser->window()->GetNativeWindow())) {
      continue;
    }

    TabStripModel* tab_strip = browser->tab_strip_model();
    if (browser->is_type_normal())
      found_tabbed_browser = true;
    if (!(event_flags & ui::EF_SHIFT_DOWN)) {
      app_menu_items.push_back({browser, kNoTab});
      auto* tab = tab_strip->GetActiveWebContents();
      const gfx::Image& icon =
          ui::ResourceBundle::GetSharedInstance().GetImageNamed(
              (browser->profile() && browser->profile()->IsIncognitoProfile())
                  ? IDR_ASH_SHELF_LIST_INCOGNITO_BROWSER
                  : IDR_ASH_SHELF_LIST_BROWSER);
      items.push_back({app_menu_items.size() - 1,
                       controller->GetAppMenuTitle(tab), icon.AsImageSkia()});
    } else {
      for (int i = 0; i < tab_strip->count(); ++i) {
        auto* tab = tab_strip->GetWebContentsAt(i);
        app_menu_items.push_back({browser, i});
        items.push_back({app_menu_items.size() - 1,
                         controller->GetAppMenuTitle(tab),
                         controller->GetAppMenuIcon(tab).AsImageSkia()});
      }
    }
  }
  // If only windowed applications are open, we return an empty list to
  // enforce the creation of a new browser.
  if (!found_tabbed_browser)
    return AppMenuItems();
  app_menu_items_ = std::move(app_menu_items);
  return items;
}

void BrowserShortcutLauncherItemController::GetContextMenu(
    int64_t display_id,
    GetContextMenuCallback callback) {
  ChromeLauncherController* controller = ChromeLauncherController::instance();
  const ash::ShelfItem* item = controller->GetItem(shelf_id());
  context_menu_ = ShelfContextMenu::Create(controller, item, display_id);
  context_menu_->GetMenuModel(std::move(callback));
}

void BrowserShortcutLauncherItemController::ExecuteCommand(
    bool from_context_menu,
    int64_t command_id,
    int32_t event_flags,
    int64_t display_id) {
  if (from_context_menu && ExecuteContextMenuCommand(command_id, event_flags))
    return;

  // Check that the index is valid and the browser has not been closed.
  // It's unclear why, but the browser's window may be null: crbug.com/937088
  if (command_id < static_cast<int64_t>(app_menu_items_.size()) &&
      app_menu_items_[command_id].first &&
      app_menu_items_[command_id].first->window()) {
    Browser* browser = app_menu_items_[command_id].first;
    TabStripModel* tab_strip = browser->tab_strip_model();
    const int tab_index = app_menu_items_[command_id].second;
    if (event_flags & (ui::EF_SHIFT_DOWN | ui::EF_MIDDLE_MOUSE_BUTTON)) {
      if (tab_index == kNoTab) {
        tab_strip->CloseAllTabs();
      } else if (tab_strip->ContainsIndex(tab_index)) {
        tab_strip->CloseWebContentsAt(tab_index,
                                      TabStripModel::CLOSE_USER_GESTURE);
      }
    } else {
      multi_user_util::MoveWindowToCurrentDesktop(
          browser->window()->GetNativeWindow());
      if (tab_index != kNoTab && tab_strip->ContainsIndex(tab_index))
        tab_strip->ActivateTabAt(tab_index);
      browser->window()->Show();
      browser->window()->Activate();
    }
  }

  app_menu_items_.clear();
}

void BrowserShortcutLauncherItemController::Close() {
  for (auto* browser : GetListOfActiveBrowsers())
    browser->window()->Close();
}

// static
bool BrowserShortcutLauncherItemController::IsListOfActiveBrowserEmpty() {
  return GetListOfActiveBrowsers().empty();
}

ash::ShelfAction
BrowserShortcutLauncherItemController::ActivateOrAdvanceToNextBrowser() {
  // Create a list of all suitable running browsers.
  std::vector<Browser*> items;
  // We use the list in the order of how the browsers got created - not the LRU
  // order.
  const BrowserList* browser_list = BrowserList::GetInstance();
  for (BrowserList::const_iterator it = browser_list->begin();
       it != browser_list->end(); ++it) {
    if (IsBrowserRepresentedInBrowserList(*it))
      items.push_back(*it);
  }
  // If there are no suitable browsers we create a new one.
  if (items.empty()) {
    ash::NewWindowDelegate::GetInstance()->NewWindow(/*incognito=*/false);
    return ash::SHELF_ACTION_NEW_WINDOW_CREATED;
  }
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  if (items.size() == 1) {
    // If there is only one suitable browser, we can either activate it, or
    // bounce it (if it is already active).
    if (items[0]->window()->IsActive()) {
      ash_util::BounceWindow(items[0]->window()->GetNativeWindow());
      return ash::SHELF_ACTION_NONE;
    }
    browser = items[0];
  } else {
    // If there is more than one suitable browser, we advance to the next if
    // |browser| is already active - or - check the last used browser if it can
    // be used.
    std::vector<Browser*>::iterator i =
        std::find(items.begin(), items.end(), browser);
    if (i != items.end()) {
      if (browser->window()->IsActive())
        browser = (++i == items.end()) ? items[0] : *i;
    } else {
      browser = chrome::FindTabbedBrowser(
          ChromeLauncherController::instance()->profile(), true);
      if (!browser || !IsBrowserRepresentedInBrowserList(browser))
        browser = items[0];
    }
  }
  DCHECK(browser);
  browser->window()->Show();
  browser->window()->Activate();
  return ash::SHELF_ACTION_WINDOW_ACTIVATED;
}

void BrowserShortcutLauncherItemController::OnBrowserAdded(Browser* browser) {
  if (!ShouldRecordLaunchTime(browser))
    return;

  const BrowserList* browser_list = BrowserList::GetInstance();
  for (BrowserList::const_iterator it = browser_list->begin();
       it != browser_list->end(); ++it) {
    if (*it == browser)
      continue;
    if (ShouldRecordLaunchTime(*it))
      return;
  }

  extensions::ExtensionPrefs::Get(browser->profile())
      ->SetLastLaunchTime(shelf_id().app_id, base::Time::Now());
}

void BrowserShortcutLauncherItemController::OnBrowserClosing(Browser* browser) {
  DCHECK(browser);
  // Reset pointers to the closed browser, but leave menu indices intact.
  for (auto& it : app_menu_items_) {
    if (it.first == browser)
      it.first = nullptr;
  }
}
