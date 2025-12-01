// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/browser_shortcut_shelf_item_controller.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_animations.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/ash/browser_delegate/browser_controller.h"
#include "chrome/browser/ash/browser_delegate/browser_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/shelf/shelf_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/app_constants/constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_prefs.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/image/image.h"

// Maintains an ordered list of browsers for shelf operations.
class BrowserShortcutShelfItemController::ShelfItemBrowsers
    : public ash::BrowserController::Observer {
 public:
  explicit ShelfItemBrowsers(const ash::ShelfModel* shelf_model)
      : shelf_model_(shelf_model) {
    browser_observation_.Observe(ash::BrowserController::GetInstance());

    // Capture initial browser list ordered by activation time. Browser
    // iterator APIs do not support creation order.
    ash::BrowserController::GetInstance()->ForEachBrowser(
        ash::BrowserController::BrowserOrder::kAscendingActivationTime,
        [&](ash::BrowserDelegate& delegate) {
          ordered_browsers_.push_back(&delegate);
          return ash::BrowserController::kContinueIteration;
        });
  }

  ShelfItemBrowsers(const ShelfItemBrowsers&) = delete;
  ShelfItemBrowsers& operator=(const ShelfItemBrowsers&) = delete;
  ~ShelfItemBrowsers() override = default;

  // Returns all available browsers, filtered for shelf representation. The
  // order is maintained consistently between calls.
  std::vector<ash::BrowserDelegate*> GetFilteredBrowsers() const {
    std::vector<ash::BrowserDelegate*> result;
    for (ash::BrowserDelegate* browser : ordered_browsers_) {
      if (IsBrowserRepresentedInBrowserList(browser, shelf_model_)) {
        result.push_back(browser);
      }
    }
    return result;
  }

  // ash::BrowserController::Observer:
  void OnBrowserCreated(ash::BrowserDelegate* browser) override {
    // New browsers are most recently activated, add to end.
    ordered_browsers_.push_back(browser);
  }

  void OnBrowserClosed(ash::BrowserDelegate* browser) override {
    std::erase(ordered_browsers_, browser);
  }

 private:
  const raw_ptr<const ash::ShelfModel> shelf_model_;
  std::vector<raw_ptr<ash::BrowserDelegate>> ordered_browsers_;
  base::ScopedObservation<ash::BrowserController,
                          ash::BrowserController::Observer>
      browser_observation_{this};
};

namespace {

// The tab-index flag for browser window menu items that do not specify a tab.
constexpr int kNoTab = std::numeric_limits<int>::max();

// Gets a list of active browsers.
std::vector<ash::BrowserDelegate*> GetListOfActiveBrowsers(
    const ash::ShelfModel* model) {
  std::vector<ash::BrowserDelegate*> active_browsers;
  ash::BrowserController::GetInstance()->ForEachBrowser(
      ash::BrowserController::BrowserOrder::kAscendingCreationTime,
      [&](ash::BrowserDelegate& browser) {
        if (browser.GetAccountId() != GetActiveAccountId()) {
          return ash::BrowserController::kContinueIteration;
        }

        // Exclude invisible non-minimized browser windows on the active desk.
        if (!browser.IsVisible() && !browser.IsMinimized() &&
            ash::desks_util::BelongsToActiveDesk(browser.GetNativeWindow())) {
          return ash::BrowserController::kContinueIteration;
        }

        if (!IsBrowserRepresentedInBrowserList(&browser, model) &&
            browser.GetType() != ash::BrowserType::kNormal) {
          return ash::BrowserController::kContinueIteration;
        }

        active_browsers.push_back(&browser);
        return ash::BrowserController::kContinueIteration;
      });
  return active_browsers;
}

bool ShouldRecordLaunchTime(const ash::BrowserDelegate& browser,
                            const ash::ShelfModel* model) {
  return !browser.IsOffTheRecord() &&
         IsBrowserRepresentedInBrowserList(&browser, model);
}

}  // namespace

BrowserShortcutShelfItemController::BrowserShortcutShelfItemController(
    ash::ShelfModel* shelf_model)
    : ash::ShelfItemDelegate(ash::ShelfID(app_constants::kChromeAppId)),
      shelf_model_(shelf_model),
      shelf_browsers_(std::make_unique<ShelfItemBrowsers>(shelf_model)) {
  browser_controller_observation_.Observe(
      ash::BrowserController::GetInstance());
}

BrowserShortcutShelfItemController::~BrowserShortcutShelfItemController() =
    default;

// This function is responsible for handling mouse and key events that are
// triggered when Ash is the Chrome browser and when the browser icon on the
// shelf is clicked, or when the Alt+N accelerator is triggered for the
// browser. For SWA and PWA please refer to AppShortcutShelfItemController.
void BrowserShortcutShelfItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback,
    const ItemFilterPredicate& filter_predicate) {
  Profile* profile = ChromeShelfController::instance()->profile();
  if (auto* full_restore_service =
          ash::full_restore::FullRestoreServiceFactory::GetForProfile(
              profile)) {
    full_restore_service->MaybeCloseNotification();
  }

  if (event && (event->flags() & ui::EF_CONTROL_DOWN)) {
    ash::NewWindowDelegate::GetInstance()->NewWindow(
        /*incognito=*/false,
        /*should_trigger_session_restore=*/true);
    std::move(callback).Run(ash::SHELF_ACTION_NEW_WINDOW_CREATED, {});
    return;
  }

  auto items =
      GetAppMenuItems(event ? event->flags() : ui::EF_NONE, filter_predicate);

  // In case of a keyboard event, we were called by a hotkey. In that case we
  // activate the next item in line if an item of our list is already active.
  //
  // Here we check the implicit assumption that the type of the event that gets
  // passed in is never ui::EventType::kKeyPressed. One may find it strange as
  // usually ui::EventType::kKeyReleased comes in pair with
  // ui::EventType::kKeyPressed, i.e, if we need to handle
  // ui::EventType::kKeyReleased, then we probably need to handle
  // ui::EventType::kKeyPressed too. However this is not the case here. The
  // ui::KeyEvent that gets passed in is manufactured as an
  // ui::EventType::kKeyReleased typed KeyEvent right before being passed in.
  // This is similar to the situations of AppShortcutShelfItemController and
  // BrowserAppShelfItemController.
  //
  // One other thing regarding the KeyEvent here that one may find confusing is
  // that even though the code here says EventType::kKeyReleased, one only needs
  // to conduct a press action (e.g., pressing Alt+1 on a physical device
  // without letting go) to trigger this ItemSelected() function call. The
  // subsequent key release action is not required. This naming disparity comes
  // from the fact that while the key accelerator is triggered and handled by
  // ui::AcceleratorManager::Process() with a KeyEvent instance as one of its
  // inputs, further down the callstack, the same KeyEvent instance is not
  // passed over into ash::Shelf::ActivateShelfItemOnDisplay(). Instead, a new
  // KeyEvent instance is fabricated inside
  // ash::Shelf::ActivateShelfItemOnDisplay(), with its type being
  // EventType::kKeyReleased, to represent the original KeyEvent, whose type is
  // EventType::kKeyPressed.
  //
  // The fabrication of the release typed key event was first introduced in this
  // CL in 2013.
  // https://chromiumcodereview.appspot.com/14551002/patch/41001/42001
  //
  // That said, there also exist other UX where the original KeyEvent instance
  // gets passed down intact. And in those UX, we should still expect a
  // EventType::kKeyPressed type. This type of UX can happen when the user keeps
  // pressing the Tab key to move to the next icon, and then presses the Enter
  // key to launch the app. It can also happen in a ChromeVox session, in which
  // the Space key can be used to activate the app. More can be found in this
  // bug. http://b/315364997.
  //
  // A bug is filed to track future works for fixing this confusing naming
  // disparity. https://crbug.com/1473895
  if (event && event->type() == ui::EventType::kKeyReleased) {
    std::move(callback).Run(ActivateOrAdvanceToNextBrowser(), std::move(items));
    return;
  }

  ash::BrowserDelegate* last_browser =
      ash::BrowserController::GetInstance()->GetDelegate(
          chrome::FindTabbedBrowser(profile, true));

  if (last_browser && !filter_predicate.is_null() &&
      !filter_predicate.Run(last_browser->GetNativeWindow())) {
    last_browser = nullptr;
  }

  if (!last_browser) {
    ash::NewWindowDelegate::GetInstance()->NewWindow(
        /*incognito=*/false,
        /*should_trigger_session_restore=*/true);
    std::move(callback).Run(ash::SHELF_ACTION_NEW_WINDOW_CREATED, {});
    return;
  }

  ash::ShelfAction action;
  if (items.size() == 1) {
    // Single browser, activate or minimize if active.
    action =
        ChromeShelfController::instance()->ActivateWindowOrMinimizeIfActive(
            last_browser->GetWindow(), true /* minimize allowed */);
  } else if (source == ash::LAUNCH_FROM_SHELF) {
    // Multiple targets, activating from shelf, a menu will be shown.
    // No need to activate or minimize the recently active browser.
    action = ash::SHELF_ACTION_NONE;
  } else {
    // Multiple targets, not activating from shelf, no menu will be shown.
    // Activate the recently active browser, never minimize.
    action =
        ChromeShelfController::instance()->ActivateWindowOrMinimizeIfActive(
            last_browser->GetWindow(), false /* minimize not allowed */);
  }
  std::move(callback).Run(action, std::move(items));
}

ash::ShelfItemDelegate::AppMenuItems
BrowserShortcutShelfItemController::GetAppMenuItems(
    int event_flags,
    const ItemFilterPredicate& filter_predicate) {
  std::vector<std::pair<Browser*, size_t>> app_menu_items;
  AppMenuItems items;
  bool found_tabbed_browser = false;
  ChromeShelfController* controller = ChromeShelfController::instance();

  for (ash::BrowserDelegate* browser : GetListOfActiveBrowsers(shelf_model_)) {
    if (!filter_predicate.is_null() &&
        !filter_predicate.Run(browser->GetNativeWindow())) {
      continue;
    }

    if (browser->GetType() == ash::BrowserType::kNormal) {
      found_tabbed_browser = true;
    }

    if (!(event_flags & ui::EF_SHIFT_DOWN)) {
      base::RecordAction(base::UserMetricsAction(
          "Shelf_BrowserShortcutShelfItem_ShowWindows"));
      app_menu_items.emplace_back(&browser->GetBrowser(), kNoTab);
      auto* tab = browser->GetActiveWebContents();
      const gfx::Image& icon =
          ui::ResourceBundle::GetSharedInstance().GetImageNamed(
              (browser->GetBrowser().profile() &&
               browser->GetBrowser().profile()->IsIncognitoProfile())
                  ? IDR_ASH_SHELF_LIST_INCOGNITO_BROWSER
                  : IDR_ASH_SHELF_LIST_BROWSER);

      // Set the title of the app menu item to the browser window title if the
      // user set one on the window. Otherwise, use the title defined in
      // ChromeShelfController.
      std::string browser_title = browser->GetBrowser().user_title();
      std::u16string item_title = browser_title.empty()
                                      ? controller->GetAppMenuTitle(tab)
                                      : base::UTF8ToUTF16(browser_title);

      items.push_back({static_cast<int>(app_menu_items.size() - 1), item_title,
                       icon.AsImageSkia()});
    } else {
      base::RecordAction(
          base::UserMetricsAction("Shelf_BrowserShortcutShelfItem_ShowTabs"));
      for (size_t i = 0; i < browser->GetWebContentsCount(); ++i) {
        auto* tab = browser->GetWebContentsAt(i);
        app_menu_items.emplace_back(&browser->GetBrowser(), i);
        items.push_back({static_cast<int>(app_menu_items.size() - 1),
                         controller->GetAppMenuTitle(tab),
                         controller->GetAppMenuIcon(tab).AsImageSkia()});
      }
    }
  }
  // If only windowed applications are open, we return an empty list to
  // enforce the creation of a new browser.
  if (!found_tabbed_browser) {
    return AppMenuItems();
  }
  app_menu_items_ = std::move(app_menu_items);
  return items;
}

void BrowserShortcutShelfItemController::GetContextMenu(
    int64_t display_id,
    GetContextMenuCallback callback) {
  ChromeShelfController* controller = ChromeShelfController::instance();
  const ash::ShelfItem* item = controller->GetItem(shelf_id());
  context_menu_ = ShelfContextMenu::Create(controller, item, display_id);
  context_menu_->GetMenuModel(std::move(callback));
}

void BrowserShortcutShelfItemController::ExecuteCommand(bool from_context_menu,
                                                        int64_t command_id,
                                                        int32_t event_flags,
                                                        int64_t display_id) {
  DCHECK(!from_context_menu);

  // Check that the index is valid and the browser has not been closed.
  // It's unclear why, but the browser's window may be null: crbug.com/937088
  if (command_id < static_cast<int64_t>(app_menu_items_.size()) &&
      app_menu_items_[command_id].first &&
      app_menu_items_[command_id].first->window()) {
    ash::BrowserDelegate* browser =
        ash::BrowserController::GetInstance()->GetDelegate(
            app_menu_items_[command_id].first);
    TabStripModel* tab_strip = browser->GetBrowser().tab_strip_model();
    const int tab_index = app_menu_items_[command_id].second;
    if (event_flags & (ui::EF_SHIFT_DOWN | ui::EF_MIDDLE_MOUSE_BUTTON)) {
      if (tab_index == kNoTab) {
        tab_strip->CloseAllTabs();
      } else if (tab_strip->ContainsIndex(tab_index)) {
        browser->CloseWebContentsAt(tab_index,
                                    ash::BrowserDelegate::UserGesture::kYes);
      }
    } else {
      if (tab_index != kNoTab && tab_strip->ContainsIndex(tab_index)) {
        tab_strip->ActivateTabAt(tab_index);
      }
      ShowAndActivateBrowser(/*move_to_current_desktop=*/true, browser);
    }
  }

  app_menu_items_.clear();
}

void BrowserShortcutShelfItemController::Close() {
  for (ash::BrowserDelegate* browser : GetListOfActiveBrowsers(shelf_model_)) {
    browser->Close();
  }
}

// static
bool BrowserShortcutShelfItemController::IsListOfActiveBrowserEmpty(
    const ash::ShelfModel* model) {
  return GetListOfActiveBrowsers(model).empty();
}

ash::ShelfAction
BrowserShortcutShelfItemController::ActivateOrAdvanceToNextBrowser() {
  std::vector<ash::BrowserDelegate*> browsers =
      shelf_browsers_->GetFilteredBrowsers();

  // If there are no suitable browsers we create a new one.
  if (browsers.empty()) {
    ash::NewWindowDelegate::GetInstance()->NewWindow(
        /*incognito=*/false,
        /*should_trigger_session_restore=*/true);
    return ash::SHELF_ACTION_NEW_WINDOW_CREATED;
  }

  ash::BrowserDelegate* browser;
  if (browsers.size() == 1) {
    // If there is only one suitable browser, we can either activate it, or
    // bounce it (if it is already active).
    if (browsers[0]->IsActive()) {
      ash::BounceWindow(browsers[0]->GetNativeWindow());
      return ash::SHELF_ACTION_NONE;
    }
    browser = browsers[0];
  } else {
    // If there is more than one suitable browser, we advance to the next if
    // |browser| is already active - or - check the last used browser if it
    // can be used.
    browser = ash::BrowserController::GetInstance()->GetLastUsedBrowser();
    if (auto it = std::ranges::find(browsers, browser); it != browsers.end()) {
      if (browser->IsActive()) {
        browser = (++it == browsers.end()) ? browsers[0] : *it;
      }
    } else {
      browser = ash::BrowserController::GetInstance()->GetDelegate(
          chrome::FindTabbedBrowser(
              ChromeShelfController::instance()->profile(), true));
      if (!browser ||
          !IsBrowserRepresentedInBrowserList(browser, shelf_model_)) {
        browser = browsers[0];
      }
    }
  }
  DCHECK(browser);
  ShowAndActivateBrowser(/*move_to_current_desktop=*/false, browser);
  return ash::SHELF_ACTION_WINDOW_ACTIVATED;
}

void BrowserShortcutShelfItemController::OnBrowserCreated(
    ash::BrowserDelegate* browser) {
  if (!ShouldRecordLaunchTime(*browser, shelf_model_)) {
    return;
  }

  bool browser_found = false;
  ash::BrowserController::GetInstance()->ForEachBrowser(
      ash::BrowserController::BrowserOrder::kAscendingCreationTime,
      [&](ash::BrowserDelegate& b) {
        if (&b != browser && ShouldRecordLaunchTime(b, shelf_model_)) {
          browser_found = true;
          return ash::BrowserController::kBreakIteration;
        }
        return ash::BrowserController::kContinueIteration;
      });

  if (!browser_found) {
    extensions::ExtensionPrefs::Get(browser->GetBrowser().profile())
        ->SetLastLaunchTime(shelf_id().app_id, base::Time::Now());
  }
}

void BrowserShortcutShelfItemController::OnBrowserClosed(
    ash::BrowserDelegate* browser) {
  // Reset pointers to the closed browser, but leave menu indices intact.
  for (auto& it : app_menu_items_) {
    if (it.first == &browser->GetBrowser()) {
      it.first = nullptr;
    }
  }
}
