// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/browser_shortcut_launcher_item_controller.h"

#include <limits>
#include <utility>
#include <vector>

#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"
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
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/image/image.h"

namespace {

// The maximum number of browser or tab items supported in the application menu.
constexpr uint16_t kMaxItems = std::numeric_limits<uint16_t>::max();
// The tab-index flag for browser window menu items that do not specify a tab.
constexpr uint16_t kNoTab = std::numeric_limits<uint16_t>::max();

bool IsSettingsBrowser(Browser* browser) {
  // Normally this test is sufficient. TODO(stevenjb): Replace this with a
  // better mechanism (Settings WebUI or Browser type).
  if (chrome::IsTrustedPopupWindowWithScheme(browser, content::kChromeUIScheme))
    return true;
  // If a settings window navigates away from a kChromeUIScheme (e.g. after a
  // crash), the above may not be true, so also test against the known list
  // of settings browsers (which will not be valid during chrome::Navigate
  // which is why we still need the above test).
  if (chrome::SettingsWindowManager::GetInstance()->IsSettingsBrowser(browser))
    return true;
  return false;
}

// Returns a 32-bit command id from 16-bit browser and web-contents indices.
uint32_t GetCommandId(uint16_t browser_index, uint16_t web_contents_index) {
  return (browser_index << 16) | web_contents_index;
}

// Get the 16-bit browser index from a 32-bit command id.
uint16_t GetBrowserIndex(uint32_t command_id) {
  return base::checked_cast<uint16_t>((command_id >> 16) & 0xFFFF);
}

// Get the 16-bit web-contents index from a 32-bit command id.
uint16_t GetWebContentsIndex(uint32_t command_id) {
  return base::checked_cast<uint16_t>(command_id & 0xFFFF);
}

// Check if the given |web_contents| is in incognito mode.
bool IsIncognito(content::WebContents* web_contents) {
  const Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  return profile->IsOffTheRecord() && !profile->IsGuestSession();
}

// Get the favicon for the browser list entry for |web_contents|.
// Note that for incognito windows the incognito icon will be returned.
gfx::Image GetBrowserListIcon(content::WebContents* web_contents) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetImageNamed(IsIncognito(web_contents)
                              ? IDR_ASH_SHELF_LIST_INCOGNITO_BROWSER
                              : IDR_ASH_SHELF_LIST_BROWSER);
}

// Get the title for the browser list entry for |web_contents|.
// If |web_contents| has not loaded, returns "New Tab".
base::string16 GetBrowserListTitle(content::WebContents* web_contents) {
  const base::string16& title = web_contents->GetTitle();
  return title.empty() ? l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE) : title;
}

}  // namespace

BrowserShortcutLauncherItemController::BrowserShortcutLauncherItemController(
    ash::ShelfModel* shelf_model)
    : ash::ShelfItemDelegate(ash::ShelfID(extension_misc::kChromeAppId)),
      shelf_model_(shelf_model),
      browser_list_observer_(this) {
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
    ~BrowserShortcutLauncherItemController() {}

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
      !multi_user_util::IsProfileFromActiveUser(browser->profile()) ||
      IsSettingsBrowser(browser)) {
    return;
  }

  const ash::ShelfID shelf_id =
      ChromeLauncherController::instance()->GetShelfIDForWebContents(
          web_contents);
  browser->window()->GetNativeWindow()->SetProperty(
      ash::kShelfIDKey, new std::string(shelf_id.Serialize()));
}

void BrowserShortcutLauncherItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback) {
  if (event && (event->flags() & ui::EF_CONTROL_DOWN)) {
    chrome::NewEmptyWindow(ChromeLauncherController::instance()->profile());
    std::move(callback).Run(ash::SHELF_ACTION_NEW_WINDOW_CREATED,
                            base::nullopt);
    return;
  }

  ash::MenuItemList items =
      GetAppMenuItems(event ? event->flags() : ui::EF_NONE);

  // In case of a keyboard event, we were called by a hotkey. In that case we
  // activate the next item in line if an item of our list is already active.
  if (event && event->type() == ui::ET_KEY_RELEASED) {
    std::move(callback).Run(ActivateOrAdvanceToNextBrowser(), std::move(items));
    return;
  }

  Profile* profile = ChromeLauncherController::instance()->profile();
  Browser* last_browser = chrome::FindTabbedBrowser(profile, true);

  if (!last_browser) {
    chrome::NewEmptyWindow(profile);
    std::move(callback).Run(ash::SHELF_ACTION_NEW_WINDOW_CREATED,
                            base::nullopt);
    return;
  }

  ash::ShelfAction action =
      ChromeLauncherController::instance()->ActivateWindowOrMinimizeIfActive(
          last_browser->window(), items.size() == 1);
  std::move(callback).Run(action, std::move(items));
}

ash::MenuItemList BrowserShortcutLauncherItemController::GetAppMenuItems(
    int event_flags) {
  browser_menu_items_.clear();
  browser_list_observer_.RemoveAll();

  ash::MenuItemList items;
  bool found_tabbed_browser = false;
  ChromeLauncherController* controller = ChromeLauncherController::instance();
  for (auto* browser : GetListOfActiveBrowsers()) {
    if (browser_menu_items_.size() >= kMaxItems)
      break;
    TabStripModel* tab_strip = browser->tab_strip_model();
    const int tab_index = tab_strip->active_index();
    if (tab_index < 0 || tab_index >= kMaxItems)
      continue;
    if (browser->is_type_tabbed())
      found_tabbed_browser = true;
    if (!(event_flags & ui::EF_SHIFT_DOWN)) {
      content::WebContents* tab = tab_strip->GetWebContentsAt(tab_index);
      ash::mojom::MenuItemPtr item(ash::mojom::MenuItem::New());
      item->command_id = GetCommandId(browser_menu_items_.size(), kNoTab);
      item->label = GetBrowserListTitle(tab);
      item->image = GetBrowserListIcon(tab).AsImageSkia();
      items.push_back(std::move(item));
    } else {
      for (uint16_t i = 0; i < tab_strip->count() && i < kMaxItems; ++i) {
        content::WebContents* tab = tab_strip->GetWebContentsAt(i);
        ash::mojom::MenuItemPtr item(ash::mojom::MenuItem::New());
        item->command_id = GetCommandId(browser_menu_items_.size(), i);
        item->label = controller->GetAppListTitle(tab);
        item->image = controller->GetAppListIcon(tab).AsImageSkia();
        items.push_back(std::move(item));
      }
    }
    browser_menu_items_.push_back(browser);
    if (!browser_list_observer_.IsObservingSources())
      browser_list_observer_.Add(BrowserList::GetInstance());
  }
  // If only windowed applications are open, we return an empty list to
  // enforce the creation of a new browser.
  if (!found_tabbed_browser) {
    items.clear();
    browser_menu_items_.clear();
    browser_list_observer_.RemoveAll();
  }
  return items;
}

void BrowserShortcutLauncherItemController::GetContextMenu(
    int64_t display_id,
    GetMenuModelCallback callback) {
  ChromeLauncherController* controller = ChromeLauncherController::instance();
  const ash::ShelfItem* item = controller->GetItem(shelf_id());
  context_menu_ = LauncherContextMenu::Create(controller, item, display_id);
  context_menu_->GetMenuModel(std::move(callback));
}

void BrowserShortcutLauncherItemController::ExecuteCommand(
    bool from_context_menu,
    int64_t command_id,
    int32_t event_flags,
    int64_t display_id) {
  if (from_context_menu && ExecuteContextMenuCommand(command_id, event_flags))
    return;

  const uint16_t browser_index = GetBrowserIndex(command_id);
  // Check that the index is valid and the browser has not been closed.
  if (browser_index < browser_menu_items_.size() &&
      browser_menu_items_[browser_index]) {
    Browser* browser = browser_menu_items_[browser_index];
    TabStripModel* tab_strip = browser->tab_strip_model();
    const uint16_t tab_index = GetWebContentsIndex(command_id);
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
        tab_strip->ActivateTabAt(tab_index, false);
      browser->window()->Show();
      browser->window()->Activate();
    }
  }

  browser_menu_items_.clear();
  browser_list_observer_.RemoveAll();
}

void BrowserShortcutLauncherItemController::Close() {
  for (auto* browser : GetListOfActiveBrowsers())
    browser->window()->Close();
}

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
    chrome::NewEmptyWindow(ChromeLauncherController::instance()->profile());
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

bool BrowserShortcutLauncherItemController::IsBrowserRepresentedInBrowserList(
    Browser* browser) {
  // Only Ash desktop browser windows for the active user are represented.
  if (!browser || !multi_user_util::IsProfileFromActiveUser(browser->profile()))
    return false;

  if (browser->is_app() && browser->is_type_popup()) {
    // Crostini Terminals always have their own item.
    // TODO(rjwright): We shouldn't need to special-case Crostini here.
    // https://crbug.com/846546
    if (crostini::CrostiniAppIdFromAppName(browser->app_name()))
      return false;

    // V1 App popup windows may have their own item.
    ash::ShelfID id(web_app::GetAppIdFromApplicationName(browser->app_name()));
    if (ChromeLauncherController::instance()->GetItem(id) != nullptr)
      return false;
  }

  // Settings browsers have their own item; all others should be represented.
  return !IsSettingsBrowser(browser);
}

BrowserList::BrowserVector
BrowserShortcutLauncherItemController::GetListOfActiveBrowsers() {
  BrowserList::BrowserVector active_browsers;
  for (auto* browser : *BrowserList::GetInstance()) {
    // Make sure that the browser is from the current user, has a proper window,
    // and the window was already shown.
    if (!multi_user_util::IsProfileFromActiveUser(browser->profile()))
      continue;
    if (!browser->window()->GetNativeWindow()->IsVisible() &&
        !browser->window()->IsMinimized()) {
      continue;
    }
    if (!IsBrowserRepresentedInBrowserList(browser) &&
        !browser->is_type_tabbed())
      continue;
    active_browsers.push_back(browser);
  }
  return active_browsers;
}

void BrowserShortcutLauncherItemController::OnBrowserClosing(Browser* browser) {
  DCHECK(browser);
  BrowserList::BrowserVector::iterator item = std::find(
      browser_menu_items_.begin(), browser_menu_items_.end(), browser);
  // Clear the entry for the closed browser and leave other indices intact.
  if (item != browser_menu_items_.end())
    *item = nullptr;
}
