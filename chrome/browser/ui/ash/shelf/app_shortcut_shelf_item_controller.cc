// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_shortcut_shelf_item_controller.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "ash/public/cpp/shelf_types.h"
#include "ash/wm/window_animations.h"
#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/shelf/shelf_context_menu.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "components/app_constants/constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"

using extensions::Extension;
using extensions::ExtensionRegistry;

namespace {

// The time delta between clicks in which clicks to launch V2 apps are ignored.
const int kClickSuppressionInMS = 1000;

bool IsAppBrowser(Browser* browser) {
  return browser->is_type_app() || browser->is_type_app_popup();
}

// Activate the browser with the given |content| and show the associated tab,
// or minimize the browser if it is already active. Returns the action
// performed by activating the content.
ash::ShelfAction ActivateContentOrMinimize(content::WebContents* content,
                                           bool allow_minimize) {
  Browser* browser = chrome::FindBrowserWithTab(content);
  TabStripModel* tab_strip = browser->tab_strip_model();
  int index = tab_strip->GetIndexOfWebContents(content);
  DCHECK_NE(TabStripModel::kNoTab, index);

  int old_index = tab_strip->active_index();
  if (index != old_index)
    tab_strip->ActivateTabAt(index);
  return ChromeShelfController::instance()->ActivateWindowOrMinimizeIfActive(
      browser->window(), index == old_index && allow_minimize);
}

// Advance to the next window of an app if possible. |items| is the list of
// browsers/web contents associated with this app. |active_item_callback|
// retrieves the window that is currently active, if available.
// |activate_callback| will activate the next window selected by this function.
template <class T>
std::optional<ash::ShelfAction> AdvanceApp(
    const std::vector<raw_ptr<T, VectorExperimental>>& items,
    base::OnceCallback<T*(const std::vector<raw_ptr<T, VectorExperimental>>&,
                          aura::Window**)> active_item_callback,
    base::OnceCallback<void(T*)> activate_callback) {
  if (items.empty())
    return std::nullopt;

  // Get the active item and associated aura::Window if it exists.
  aura::Window* active_item_window = nullptr;
  T* active_item =
      std::move(active_item_callback).Run(items, &active_item_window);

  // If there is only one of the app, and it is the current active item,
  // bounce the window to signal nothing happened.
  if (items.size() == 1u && active_item) {
    DCHECK(active_item_window);
    ash::BounceWindow(active_item_window);
    return ash::SHELF_ACTION_NONE;
  }

  // If one of the items is active, active the next one in the list, otherwise
  // activate the first item in the list.
  size_t index = 0;
  if (active_item) {
    DCHECK(base::Contains(items, active_item));
    auto it = base::ranges::find(items, active_item);
    index = (it - items.cbegin() + 1) % items.size();
  }
  std::move(activate_callback).Run(items[index]);
  return ash::SHELF_ACTION_WINDOW_ACTIVATED;
}

// AppMatcher is used to determine if various WebContents instances are
// associated with a specific app. Clients should call CanMatchWebContents()
// before iterating through WebContents instances and calling
// WebContentMatchesApp().
class AppMatcher {
 public:
  AppMatcher(Profile* profile,
             const std::string& app_id,
             const URLPattern& refocus_pattern)
      : app_id_(app_id), refocus_pattern_(refocus_pattern) {
    DCHECK(profile);
    if (web_app::WebAppProvider* provider =
            web_app::WebAppProvider::GetForLocalAppsUnchecked(profile)) {
      if (provider->registrar_unsafe().IsInstallState(
              app_id, {web_app::proto::INSTALLED_WITH_OS_INTEGRATION})) {
        registrar_ = &provider->registrar_unsafe();
      }
    }
    if (!registrar_)
      extension_ = GetExtensionForAppID(app_id, profile);
  }

  AppMatcher(const AppMatcher&) = delete;
  AppMatcher& operator=(const AppMatcher&) = delete;

  bool CanMatchWebContents() const { return registrar_ || extension_; }

  // Returns true if this app matches the given |web_contents|. If
  // the browser is an app browser, the application gets first checked against
  // its original URL since a windowed app might have navigated away from its
  // app domain.
  // May only be called if CanMatchWebContents() return true.
  bool WebContentMatchesApp(content::WebContents* web_contents,
                            Browser* browser) const {
    DCHECK(CanMatchWebContents());
    return extension_ ? WebContentMatchesHostedApp(web_contents, browser)
                      : WebContentMatchesWebApp(web_contents, browser);
  }

  bool IsAshBrowser() const { return app_id_ == app_constants::kChromeAppId; }

 private:
  bool WebContentMatchesHostedApp(content::WebContents* web_contents,
                                  Browser* browser) const {
    DCHECK(extension_);
    DCHECK(!registrar_);

    // If the browser is an app window, and the app name matches the extension,
    // then the contents match the app.
    if (IsAppBrowser(browser)) {
      const Extension* browser_extension =
          ExtensionRegistry::Get(browser->profile())
              ->GetExtensionById(
                  web_app::GetAppIdFromApplicationName(browser->app_name()),
                  ExtensionRegistry::EVERYTHING);
      return browser_extension == extension_;
    }

    // Apps set to launch in app windows should not match contents running in
    // tabs.
    if (extensions::LaunchesInWindow(browser->profile(), extension_))
      return false;

    // There are three ways to identify the association of a URL with this
    // extension:
    // - The refocus pattern is matched (needed for apps like drive).
    // - The extension's origin + extent gets matched.
    // - The shelf controller knows that the tab got created for this app.
    const GURL tab_url = web_contents->GetURL();
    return ((!refocus_pattern_.match_all_urls() &&
             refocus_pattern_.MatchesURL(tab_url)) ||
            (extension_->OverlapsWithOrigin(tab_url) &&
             extension_->web_extent().MatchesURL(tab_url)) ||
            ChromeShelfController::instance()->IsWebContentHandledByApplication(
                web_contents, app_id_));
  }

  // Returns true if this web app matches the given |web_contents|. If the
  // browser has an app controller, the application gets first checked against
  // its original URL since a windowed app might have navigated away from its
  // app domain.
  bool WebContentMatchesWebApp(content::WebContents* web_contents,
                               Browser* browser) const {
    DCHECK(registrar_);
    DCHECK(!extension_);

    // If the browser is a web app window, and the window app id matches,
    // then the contents match the app.
    if (browser->app_controller())
      return browser->app_controller()->app_id() == app_id_;

    // There are three ways to identify the association of a URL with this
    // web app:
    // - The refocus pattern is matched (needed for apps like drive).
    // - The web app's scope gets matched.
    // - The shelf controller knows that the tab got created for this web app.
    const GURL tab_url = web_contents->GetURL();
    std::optional<GURL> app_scope = registrar_->GetAppScope(app_id_);
    DCHECK(app_scope.has_value());

    return ((!refocus_pattern_.match_all_urls() &&
             refocus_pattern_.MatchesURL(tab_url)) ||
            (base::StartsWith(tab_url.spec(), app_scope->spec(),
                              base::CompareCase::SENSITIVE)) ||
            ChromeShelfController::instance()->IsWebContentHandledByApplication(
                web_contents, app_id_));
  }

  const std::string app_id_;
  const URLPattern refocus_pattern_;

  // AppMatcher is stack allocated. Pointer members below are not owned.

  // registrar_ is set when app_id_ is a web app.
  raw_ptr<const web_app::WebAppRegistrar> registrar_ = nullptr;

  // extension_ is set when app_id_ is a hosted app.
  raw_ptr<const Extension> extension_ = nullptr;
};

}  // namespace

AppShortcutShelfItemController::AppShortcutShelfItemController(
    const ash::ShelfID& shelf_id)
    : ash::ShelfItemDelegate(shelf_id) {
  BrowserList::AddObserver(this);

  // To detect V1 applications we use their domain and match them against the
  // used URL. This will also work with applications like Google Drive.
  const Extension* extension = GetExtensionForAppID(
      shelf_id.app_id, ChromeShelfController::instance()->profile());
  // Some unit tests have no real extension.
  if (extension) {
    set_refocus_url(GURL(
        extensions::AppLaunchInfo::GetLaunchWebURL(extension).spec() + "*"));
  }
}

AppShortcutShelfItemController::~AppShortcutShelfItemController() {
  BrowserList::RemoveObserver(this);
}

// This function is responsible for handling mouse and key events that are
// triggered when Ash is the Chrome browser and when an SWA or PWA icon on
// the shelf is clicked, or when the Alt+N accelerator is triggered for the
// SWA or PWA. For Ash-chrome please refer to
// BrowserShortcutShelfItemController. For Lacros please refer to
// BrowserAppShelfItemController.
void AppShortcutShelfItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback,
    const ItemFilterPredicate& filter_predicate) {
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
  // This is similar to the situations of BrowserShortcutShelfItemController and
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
    auto optional_action = AdvanceToNextApp(filter_predicate);
    if (optional_action.has_value()) {
      std::move(callback).Run(optional_action.value(), {});
      return;
    }
  }

  AppMenuItems items =
      GetAppMenuItems(event ? event->flags() : ui::EF_NONE, filter_predicate);
  if (items.empty()) {
    // Ideally we come here only once. After that ShellLauncherItemController
    // will take over when the shell window gets opened. However there are apps
    // which take a lot of time for pre-processing (like the files app) before
    // they open a window. Since there is currently no other way to detect if an
    // app was started we suppress any further clicks within a special time out.
    if (IsV2App() && !AllowNextLaunchAttempt()) {
      std::move(callback).Run(ash::SHELF_ACTION_NONE, std::move(items));
      return;
    }

    // LaunchApp may replace and destroy this item controller instance. Run the
    // callback first and copy the id to avoid crashes.
    std::move(callback).Run(ash::SHELF_ACTION_NEW_WINDOW_CREATED, {});

    ChromeShelfController* chrome_shelf_controller =
        ChromeShelfController::instance();
    MaybeRecordAppLaunchForScalableIph(
        shelf_id().app_id, chrome_shelf_controller->profile(), source);
    chrome_shelf_controller->LaunchApp(ash::ShelfID(shelf_id()), source,
                                       ui::EF_NONE, display_id);
    return;
  }

  if (source != ash::LAUNCH_FROM_SHELF || items.size() == 1) {
    const bool can_minimize = source != ash::LAUNCH_FROM_APP_LIST &&
                              source != ash::LAUNCH_FROM_APP_LIST_SEARCH;
    std::move(callback).Run(
        app_menu_cached_by_browsers_
            ? ChromeShelfController::instance()
                  ->ActivateWindowOrMinimizeIfActive(
                      // We don't need to check nullptr here because
                      // we just called GetAppMenuItems() above to update it.
                      app_menu_browsers_[0]->window(), can_minimize)
            : ActivateContentOrMinimize(app_menu_web_contents_[0],
                                        can_minimize),
        {});
  } else {
    // Multiple items, a menu will be shown. No need to activate the most
    // recently active item.
    std::move(callback).Run(ash::SHELF_ACTION_NONE, std::move(items));
  }
}

bool AppShortcutShelfItemController::HasRunningApplications() {
  return IsWindowedWebApp() ? !GetAppBrowsers(base::NullCallback()).empty()
                            : !GetAppWebContents(base::NullCallback()).empty();
}

ash::ShelfItemDelegate::AppMenuItems
AppShortcutShelfItemController::GetAppMenuItems(
    int event_flags,
    const ItemFilterPredicate& filter_predicate) {
  ChromeShelfController* controller = ChromeShelfController::instance();
  AppMenuItems items;
  auto add_menu_item = [&controller,
                        &items](content::WebContents* web_contents) {
    items.push_back({static_cast<int>(items.size()),
                     controller->GetAppMenuTitle(web_contents),
                     controller->GetAppMenuIcon(web_contents).AsImageSkia()});
  };

  if (IsWindowedWebApp() && !(event_flags & ui::EF_SHIFT_DOWN)) {
    app_menu_browsers_ = GetAppBrowsers(filter_predicate);
    app_menu_cached_by_browsers_ = true;
    for (Browser* browser : app_menu_browsers_) {
      add_menu_item(browser->tab_strip_model()->GetActiveWebContents());
    }
  } else {
    app_menu_web_contents_ = GetAppWebContents(filter_predicate);
    app_menu_cached_by_browsers_ = false;
    for (content::WebContents* web_contents : app_menu_web_contents_) {
      add_menu_item(web_contents);
    }
  }

  return items;
}

void AppShortcutShelfItemController::GetContextMenu(
    int64_t display_id,
    GetContextMenuCallback callback) {
  ChromeShelfController* controller = ChromeShelfController::instance();
  const ash::ShelfItem* item = controller->GetItem(shelf_id());
  context_menu_ = ShelfContextMenu::Create(controller, item, display_id);
  context_menu_->GetMenuModel(std::move(callback));
}

void AppShortcutShelfItemController::ExecuteCommand(bool from_context_menu,
                                                    int64_t command_id,
                                                    int32_t event_flags,
                                                    int64_t display_id) {
  DCHECK(!from_context_menu);

  if (static_cast<size_t>(command_id) >= AppMenuSize()) {
    ClearAppMenu();
    return;
  }

  bool should_close =
      event_flags & (ui::EF_SHIFT_DOWN | ui::EF_MIDDLE_MOUSE_BUTTON);
  auto activate_browser = [](Browser* browser) {
    multi_user_util::MoveWindowToCurrentDesktop(
        browser->window()->GetNativeWindow());
    browser->window()->Show();
    browser->window()->Activate();
  };

  if (app_menu_cached_by_browsers_) {
    Browser* browser = app_menu_browsers_[command_id];
    if (browser) {
      if (should_close)
        browser->tab_strip_model()->CloseAllTabs();
      else
        activate_browser(browser);
    }
  } else {
    // If the web contents was destroyed while the menu was open, then the
    // invalid pointer cached in |app_menu_web_contents_| should yield a null
    // browser or kNoTab.
    content::WebContents* web_contents = app_menu_web_contents_[command_id];
    Browser* browser = chrome::FindBrowserWithTab(web_contents);
    TabStripModel* tab_strip = browser ? browser->tab_strip_model() : nullptr;
    const int index = tab_strip ? tab_strip->GetIndexOfWebContents(web_contents)
                                : TabStripModel::kNoTab;
    if (index != TabStripModel::kNoTab) {
      if (should_close) {
        tab_strip->CloseWebContentsAt(index, TabCloseTypes::CLOSE_USER_GESTURE);
      } else {
        tab_strip->ActivateTabAt(index);
        activate_browser(browser);
      }
    }
  }

  ClearAppMenu();
}

void AppShortcutShelfItemController::Close() {
  // Close all running 'programs' of this type.
  if (IsWindowedWebApp()) {
    for (Browser* browser : GetAppBrowsers(base::NullCallback()))
      browser->tab_strip_model()->CloseAllTabs();
  } else {
    for (content::WebContents* item : GetAppWebContents(base::NullCallback())) {
      Browser* browser = chrome::FindBrowserWithTab(item);
      if (!browser ||
          !multi_user_util::IsProfileFromActiveUser(browser->profile())) {
        continue;
      }
      TabStripModel* tab_strip = browser->tab_strip_model();
      int index = tab_strip->GetIndexOfWebContents(item);
      DCHECK(index != TabStripModel::kNoTab);
      tab_strip->CloseWebContentsAt(index, TabCloseTypes::CLOSE_NONE);
    }
  }
}

void AppShortcutShelfItemController::OnBrowserClosing(Browser* browser) {
  if (!app_menu_cached_by_browsers_)
    return;
  // Reset pointers to the closed browser, but leave menu indices intact.
  auto it = base::ranges::find(app_menu_browsers_, browser);
  if (it != app_menu_browsers_.end())
    *it = nullptr;
}

std::vector<raw_ptr<content::WebContents, VectorExperimental>>
AppShortcutShelfItemController::GetAppWebContents(
    const ItemFilterPredicate& filter_predicate) {
  URLPattern refocus_pattern(URLPattern::SCHEME_ALL);
  refocus_pattern.SetMatchAllURLs(true);

  if (!refocus_url_.is_empty()) {
    refocus_pattern.SetMatchAllURLs(false);
    refocus_pattern.Parse(refocus_url_.spec());
  }

  Profile* const profile = ChromeShelfController::instance()->profile();
  AppMatcher matcher(profile, app_id(), refocus_pattern);

  std::vector<raw_ptr<content::WebContents, VectorExperimental>> items;
  // It is possible to come here while an app gets loaded.
  if (!matcher.CanMatchWebContents())
    return items;

  for (Browser* browser : *BrowserList::GetInstance()) {
    if (!filter_predicate.is_null() &&
        !filter_predicate.Run(browser->window()->GetNativeWindow())) {
      continue;
    }
    if (!multi_user_util::IsProfileFromActiveUser(browser->profile()))
      continue;
    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int index = 0; index < tab_strip->count(); index++) {
      content::WebContents* web_contents = tab_strip->GetWebContentsAt(index);
      if (matcher.IsAshBrowser() ||
          matcher.WebContentMatchesApp(web_contents, browser)) {
        items.push_back(web_contents);
      }
    }
  }
  return items;
}

std::vector<raw_ptr<Browser, VectorExperimental>>
AppShortcutShelfItemController::GetAppBrowsers(
    const ItemFilterPredicate& filter_predicate) {
  DCHECK(IsWindowedWebApp());
  std::vector<raw_ptr<Browser, VectorExperimental>> browsers;
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (!filter_predicate.is_null() &&
        !filter_predicate.Run(browser->window()->GetNativeWindow())) {
      continue;
    }
    if (!multi_user_util::IsProfileFromActiveUser(browser->profile()))
      continue;
    if (!IsAppBrowser(browser))
      continue;

    if (web_app::GetAppIdFromApplicationName(browser->app_name()) == app_id() &&
        browser->tab_strip_model()->GetActiveWebContents()) {
      browsers.push_back(browser);
    }
  }
  return browsers;
}

std::optional<ash::ShelfAction>
AppShortcutShelfItemController::AdvanceToNextApp(
    const ItemFilterPredicate& filter_predicate) {
  if (!chrome::FindLastActive())
    return std::nullopt;

  if (IsWindowedWebApp()) {
    return AdvanceApp(
        GetAppBrowsers(filter_predicate),
        base::BindOnce(
            [](const std::vector<raw_ptr<Browser, VectorExperimental>>&
                   browsers,
               aura::Window** out_window) -> Browser* {
              for (Browser* browser : browsers) {
                if (browser->window()->IsActive()) {
                  *out_window = browser->window()->GetNativeWindow();
                  return browser;
                }
              }
              return nullptr;
            }),
        base::BindOnce([](Browser* browser) -> void {
          browser->window()->Show();
          browser->window()->Activate();
        }));
  }

  return AdvanceApp(
      GetAppWebContents(filter_predicate),
      base::BindOnce(
          [](const std::vector<raw_ptr<content::WebContents,
                                       VectorExperimental>>& web_contents,
             aura::Window** out_window) -> content::WebContents* {
            for (content::WebContents* web_content : web_contents) {
              Browser* browser = chrome::FindBrowserWithTab(web_content);
              // The active web contents is on the active browser, and matches
              // the index of the current active tab.
              if (browser->window()->IsActive()) {
                TabStripModel* tab_strip = browser->tab_strip_model();
                int index = tab_strip->GetIndexOfWebContents(web_content);
                if (tab_strip->active_index() == index) {
                  *out_window = browser->window()->GetNativeWindow();
                  return web_content;
                }
              }
            }
            return nullptr;
          }),
      base::BindOnce([](content::WebContents* web_contents) -> void {
        ActivateContentOrMinimize(web_contents, /*allow_minimize=*/false);
      }));
}

bool AppShortcutShelfItemController::IsV2App() {
  const Extension* extension = GetExtensionForAppID(
      app_id(), ChromeShelfController::instance()->profile());
  return extension && extension->is_platform_app();
}

bool AppShortcutShelfItemController::AllowNextLaunchAttempt() {
  if (last_launch_attempt_.is_null() ||
      last_launch_attempt_ + base::Milliseconds(kClickSuppressionInMS) <
          base::Time::Now()) {
    last_launch_attempt_ = base::Time::Now();
    return true;
  }
  return false;
}

bool AppShortcutShelfItemController::IsWindowedWebApp() {
  if (web_app::WebAppProvider* provider =
          web_app::WebAppProvider::GetForLocalAppsUnchecked(
              ChromeShelfController::instance()->profile())) {
    web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();
    if (registrar.IsInstallState(
            app_id(), {web_app::proto::INSTALLED_WITH_OS_INTEGRATION})) {
      return registrar.GetAppUserDisplayMode(app_id()) !=
             web_app::mojom::UserDisplayMode::kBrowser;
    }
  }
  return false;
}

size_t AppShortcutShelfItemController::AppMenuSize() {
  return app_menu_cached_by_browsers_ ? app_menu_browsers_.size()
                                      : app_menu_web_contents_.size();
}

void AppShortcutShelfItemController::ClearAppMenu() {
  app_menu_browsers_.clear();
  app_menu_web_contents_.clear();
}
