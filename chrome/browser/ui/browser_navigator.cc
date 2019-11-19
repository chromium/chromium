// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/previews/previews_lite_page_redirect_decider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "url/url_constants.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/account_id/account_id.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#endif

using content::GlobalRequestID;
using content::NavigationController;
using content::WebContents;

class BrowserNavigatorWebContentsAdoption {
 public:
  static void AttachTabHelpers(content::WebContents* contents) {
    TabHelpers::AttachTabHelpers(contents);

    // Make the tab show up in the task manager.
    task_manager::WebContentsTags::CreateForTabContents(contents);
  }
};

namespace {

// Returns true if the specified Browser can open tabs. Not all Browsers support
// multiple tabs, such as app frames and popups. This function returns false for
// those types of Browser.
bool WindowCanOpenTabs(Browser* browser) {
  return browser->CanSupportWindowFeature(Browser::FEATURE_TABSTRIP) ||
         browser->tab_strip_model()->empty();
}

// Finds an existing Browser compatible with |profile|, making a new one if no
// such Browser is located.
Browser* GetOrCreateBrowser(Profile* profile, bool user_gesture) {
  Browser* browser = chrome::FindTabbedBrowser(profile, false);
  return browser ? browser
                 : new Browser(Browser::CreateParams(profile, user_gesture));
}

// Change some of the navigation parameters based on the particular URL.
// Currently this applies to some chrome:// pages which we always want to open
// in a non-incognito window. Note that even though a ChromeOS guest session is
// technically an incognito window, these URLs are allowed.
// Returns true on success. Otherwise, if changing params leads the browser into
// an erroneous state, returns false.
bool AdjustNavigateParamsForURL(NavigateParams* params) {
  if (params->contents_to_insert || params->switch_to_singleton_tab ||
      IsURLAllowedInIncognito(params->url, params->initiating_profile) ||
      params->initiating_profile->IsGuestSession()) {
    return true;
  }

  Profile* profile = params->initiating_profile;

  if (profile->IsOffTheRecord() ||
      params->disposition == WindowOpenDisposition::OFF_THE_RECORD) {
    profile = profile->GetOriginalProfile();

    // If incognito is forced, we punt.
    PrefService* prefs = profile->GetPrefs();
    if (prefs && IncognitoModePrefs::GetAvailability(prefs) ==
                     IncognitoModePrefs::FORCED) {
      return false;
    }

    params->disposition = WindowOpenDisposition::SINGLETON_TAB;
    params->browser = GetOrCreateBrowser(profile, params->user_gesture);
    params->window_action = NavigateParams::SHOW_WINDOW;
  }

  return true;
}

// Returns a Browser and tab index. The browser can host the navigation or
// tab addition specified in |params|.  This might just return the same
// Browser specified in |params|, or some other if that Browser is deemed
// incompatible. The tab index will be -1 unless a singleton or tab switch
// was requested, in which case it might be the target tab index, or -1
// if not found.
std::pair<Browser*, int> GetBrowserAndTabForDisposition(
    const NavigateParams& params) {
  Profile* profile = params.initiating_profile;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (params.open_pwa_window_if_possible) {
    const extensions::Extension* app = extensions::util::GetInstalledPwaForUrl(
        profile, params.url,
        extensions::LaunchContainer::kLaunchContainerWindow);
    if (app) {
      std::string app_name =
          web_app::GenerateApplicationNameFromAppId(app->id());
      return {
          new Browser(Browser::CreateParams::CreateForApp(
              app_name,
              true,  // trusted_source. Installed PWAs are considered trusted.
              params.window_bounds, profile, params.user_gesture)),
          -1};
    }
  }
#endif

  switch (params.disposition) {
    case WindowOpenDisposition::SWITCH_TO_TAB:
#if !defined(OS_ANDROID)
    {
      std::pair<Browser*, int> index =
          GetIndexAndBrowserOfExistingTab(profile, params);
      if (index.first)
        return index;
    }
#endif
      FALLTHROUGH;
    case WindowOpenDisposition::CURRENT_TAB:
      if (params.browser)
        return {params.browser, -1};
      // Find a compatible window and re-execute this command in it. Otherwise
      // re-run with NEW_WINDOW.
      return {GetOrCreateBrowser(profile, params.user_gesture), -1};
    case WindowOpenDisposition::SINGLETON_TAB: {
      int index = GetIndexOfExistingTab(params.browser, params);
      if (index >= 0)
        return {params.browser, index};
      // If this window can't open tabs, then it would load in a random
      // window, potentially opening a second copy. Instead, make an extra
      // effort to see if there's an already open copy.
      if (params.browser && !WindowCanOpenTabs(params.browser)) {
        std::pair<Browser*, int> index =
            GetIndexAndBrowserOfExistingTab(profile, params);
        if (index.first)
          return index;
      }
    }
      FALLTHROUGH;
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      // See if we can open the tab in the window this navigator is bound to.
      if (params.browser && WindowCanOpenTabs(params.browser))
        return {params.browser, -1};

      // Find a compatible window and re-execute this command in it. Otherwise
      // re-run with NEW_WINDOW.
      return {GetOrCreateBrowser(profile, params.user_gesture), -1};
    case WindowOpenDisposition::NEW_POPUP: {
      // Make a new popup window.
      // Coerce app-style if |source| represents an app.
      std::string app_name;
#if BUILDFLAG(ENABLE_EXTENSIONS)
      if (!params.extension_app_id.empty()) {
        app_name =
            web_app::GenerateApplicationNameFromAppId(params.extension_app_id);
      } else if (params.browser && !params.browser->app_name().empty()) {
        app_name = params.browser->app_name();
      } else if (params.source_contents) {
        extensions::TabHelper* extensions_tab_helper =
            extensions::TabHelper::FromWebContents(params.source_contents);
        if (extensions_tab_helper && extensions_tab_helper->is_app()) {
          app_name = web_app::GenerateApplicationNameFromAppId(
              extensions_tab_helper->GetAppId());
        }
      }
#endif
      if (app_name.empty()) {
        Browser::CreateParams browser_params(Browser::TYPE_POPUP, profile,
                                             params.user_gesture);
        browser_params.trusted_source = params.trusted_source;
        browser_params.initial_bounds = params.window_bounds;
        return {new Browser(browser_params), -1};
      }
      return {new Browser(Browser::CreateParams::CreateForApp(
                  app_name, params.trusted_source, params.window_bounds,
                  profile, params.user_gesture)),
              -1};
    }
    case WindowOpenDisposition::NEW_WINDOW:
      // Make a new normal browser window.
      return {new Browser(Browser::CreateParams(profile, params.user_gesture)),
              -1};
    case WindowOpenDisposition::OFF_THE_RECORD:
      // Make or find an incognito window.
      return {GetOrCreateBrowser(profile->GetOffTheRecordProfile(),
                                 params.user_gesture),
              -1};
    // The following types result in no navigation.
    case WindowOpenDisposition::SAVE_TO_DISK:
    case WindowOpenDisposition::IGNORE_ACTION:
      return {nullptr, -1};
    default:
      NOTREACHED();
  }
  return {nullptr, -1};
}

// Fix disposition and other parameter values depending on prevailing
// conditions.
void NormalizeDisposition(NavigateParams* params) {
  // Calculate the WindowOpenDisposition if necessary.
  if (params->browser->tab_strip_model()->empty() &&
      (params->disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB ||
       params->disposition == WindowOpenDisposition::CURRENT_TAB ||
       params->disposition == WindowOpenDisposition::SINGLETON_TAB)) {
    params->disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  }
  if (params->browser->profile()->IsOffTheRecord() &&
      params->disposition == WindowOpenDisposition::OFF_THE_RECORD) {
    params->disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  }
  if (!params->source_contents &&
      params->disposition == WindowOpenDisposition::CURRENT_TAB)
    params->disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  switch (params->disposition) {
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      // Disposition trumps add types. ADD_ACTIVE is a default, so we need to
      // remove it if disposition implies the tab is going to open in the
      // background.
      params->tabstrip_add_types &= ~TabStripModel::ADD_ACTIVE;
      break;

    case WindowOpenDisposition::NEW_WINDOW:
    case WindowOpenDisposition::NEW_POPUP: {
      // Code that wants to open a new window typically expects it to be shown
      // automatically.
      if (params->window_action == NavigateParams::NO_ACTION)
        params->window_action = NavigateParams::SHOW_WINDOW;
      FALLTHROUGH;
    }
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
    case WindowOpenDisposition::SINGLETON_TAB:
      params->tabstrip_add_types |= TabStripModel::ADD_ACTIVE;
      break;

    default:
      break;
  }
}

// Obtain the profile used by the code that originated the Navigate() request.
Profile* GetSourceProfile(NavigateParams* params) {
  // |source_site_instance| needs to be checked before |source_contents|. This
  // might matter when chrome.windows.create is used to open multiple URLs,
  // which would reuse |params| and modify |params->source_contents| across
  // navigations.
  if (params->source_site_instance) {
    return Profile::FromBrowserContext(
        params->source_site_instance->GetBrowserContext());
  }

  if (params->source_contents) {
    return Profile::FromBrowserContext(
        params->source_contents->GetBrowserContext());
  }

  return params->initiating_profile;
}

void LoadURLInContents(WebContents* target_contents,
                       const GURL& url,
                       NavigateParams* params) {
  NavigationController::LoadURLParams load_url_params(url);
  load_url_params.initiator_origin = params->initiator_origin;
  load_url_params.source_site_instance = params->source_site_instance;
  load_url_params.referrer = params->referrer;
  load_url_params.frame_name = params->frame_name;
  load_url_params.frame_tree_node_id = params->frame_tree_node_id;
  load_url_params.redirect_chain = params->redirect_chain;
  load_url_params.transition_type = params->transition;
  load_url_params.extra_headers = params->extra_headers;
  load_url_params.should_replace_current_entry =
      params->should_replace_current_entry;
  load_url_params.is_renderer_initiated = params->is_renderer_initiated;
  load_url_params.started_from_context_menu = params->started_from_context_menu;
  load_url_params.has_user_gesture = params->user_gesture;
  load_url_params.blob_url_loader_factory = params->blob_url_loader_factory;
  load_url_params.input_start = params->input_start;
  load_url_params.was_activated = params->was_activated;
  load_url_params.href_translate = params->href_translate;
  load_url_params.reload_type = params->reload_type;

  // |frame_tree_node_id| is kNoFrameTreeNodeId for main frame navigations.
  if (params->frame_tree_node_id ==
      content::RenderFrameHost::kNoFrameTreeNodeId) {
    load_url_params.navigation_ui_data =
        ChromeNavigationUIData::CreateForMainFrameNavigation(
            target_contents, params->disposition,
            PreviewsLitePageRedirectDecider::GeneratePageIdForProfile(
                GetSourceProfile(params)));
  }

  if (params->post_data) {
    load_url_params.load_type = NavigationController::LOAD_TYPE_HTTP_POST;
    load_url_params.post_data = params->post_data;
  }

  target_contents->GetController().LoadURLWithParams(load_url_params);
}

// This class makes sure the Browser object held in |params| is made visible
// by the time it goes out of scope, provided |params| wants it to be shown.
class ScopedBrowserShower {
 public:
  explicit ScopedBrowserShower(NavigateParams* params,
                               content::WebContents** contents)
      : params_(params), contents_(contents) {}
  ~ScopedBrowserShower() {
    if (params_->window_action == NavigateParams::SHOW_WINDOW_INACTIVE) {
      params_->browser->window()->ShowInactive();
    } else if (params_->window_action == NavigateParams::SHOW_WINDOW) {
      BrowserWindow* window = params_->browser->window();
      window->Show();
      // If a user gesture opened a popup window, focus the contents.
      if (params_->user_gesture &&
          params_->disposition == WindowOpenDisposition::NEW_POPUP &&
          *contents_) {
        (*contents_)->Focus();
        window->Activate();
      }
    }
  }

 private:
  NavigateParams* params_;
  content::WebContents** contents_;
  DISALLOW_COPY_AND_ASSIGN(ScopedBrowserShower);
};

std::unique_ptr<content::WebContents> CreateTargetContents(
    const NavigateParams& params,
    const GURL& url) {
  // Always create the new WebContents in a new SiteInstance (and therefore a
  // new BrowsingInstance), *unless* there's a |params.opener|.
  //
  // Note that the SiteInstance below is only for the "initial" placement of the
  // new WebContents (i.e. if subsequent navigation [including the initial
  // navigation] triggers a cross-process transfer, then the opener and new
  // contents can end up in separate processes).  This is fine, because even if
  // subsequent navigation is cross-process (i.e. cross-SiteInstance), then it
  // will stay in the same BrowsingInstance (creating frame proxies as needed)
  // preserving the requested opener relationship along the way.
  scoped_refptr<content::SiteInstance> initial_site_instance_for_new_contents =
      params.opener
          ? params.opener->GetSiteInstance()
          : tab_util::GetSiteInstanceForNewTab(params.browser->profile(), url);

  WebContents::CreateParams create_params(
      params.browser->profile(), initial_site_instance_for_new_contents);
  create_params.main_frame_name = params.frame_name;
  if (params.opener) {
    create_params.opener_render_frame_id = params.opener->GetRoutingID();
    create_params.opener_render_process_id =
        params.opener->GetProcess()->GetID();
  }
  if (params.source_contents) {
    create_params.created_with_opener = params.created_with_opener;
  }
  if (params.disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB)
    create_params.initially_hidden = true;

#if defined(USE_AURA)
  if (params.browser->window() && params.browser->window()->GetNativeWindow())
    create_params.context = params.browser->window()->GetNativeWindow();
#endif

  std::unique_ptr<WebContents> target_contents =
      WebContents::Create(create_params);

  // New tabs can have WebUI URLs that will make calls back to arbitrary
  // tab helpers, so the entire set of tab helpers needs to be set up
  // immediately.
  BrowserNavigatorWebContentsAdoption::AttachTabHelpers(target_contents.get());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::TabHelper::FromWebContents(target_contents.get())
      ->SetExtensionAppById(params.extension_app_id);
#endif

  return target_contents;
}

// If a prerendered page exists for |url|, then replace
// params.contents_being_navigated with it. When this occurs, the new page is
// stored in params.replaced_contents.
// This method updates the underlying storage mechanism as well. e.g. On
// Desktop, |contents_being_navigated| is replaced in the tabstrip by
// |replaced_contents|.
bool SwapInPrerender(const GURL& url,
                     prerender::PrerenderManager::Params* params) {
  Profile* profile = Profile::FromBrowserContext(
      params->contents_being_navigated->GetBrowserContext());
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(profile);
  return prerender_manager &&
         prerender_manager->MaybeUsePrerenderedPage(url, params);
}

}  // namespace

void Navigate(NavigateParams* params) {
  Browser* source_browser = params->browser;
  if (source_browser)
    params->initiating_profile = source_browser->profile();
  DCHECK(params->initiating_profile);

  if (!AdjustNavigateParamsForURL(params))
    return;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(params->initiating_profile)
          ->enabled_extensions()
          .GetExtensionOrAppByURL(params->url);
  // Platform apps cannot navigate. Block the request.
  if (extension && extension->is_platform_app())
    params->url = GURL(chrome::kExtensionInvalidRequestURL);
#endif

  if (source_browser &&
      platform_util::IsBrowserLockedFullscreen(source_browser)) {
    // Block any navigation requests in locked fullscreen mode.
    return;
  }

  // Trying to open a background tab when in an app browser results in
  // focusing a regular browser window an opening a tab in the background
  // of that window. Change the disposition to NEW_FOREGROUND_TAB so that
  // the new tab is focused.
  if (source_browser && source_browser->deprecated_is_app() &&
      params->disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    params->disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  }

  // If no source WebContents was specified, we use the selected one from
  // the target browser. This must happen first, before
  // GetBrowserForDisposition() has a chance to replace |params->browser| with
  // another one.
  if (!params->source_contents && params->browser) {
    params->source_contents =
        params->browser->tab_strip_model()->GetActiveWebContents();
  }

  WebContents* contents_to_navigate_or_insert =
      params->contents_to_insert.get();
  if (params->switch_to_singleton_tab) {
    DCHECK_EQ(params->disposition, WindowOpenDisposition::SINGLETON_TAB);
    contents_to_navigate_or_insert = params->switch_to_singleton_tab;
  }
  int singleton_index;
  std::tie(params->browser, singleton_index) =
      GetBrowserAndTabForDisposition(*params);
  if (!params->browser)
    return;
  if (singleton_index != -1) {
    contents_to_navigate_or_insert =
        params->browser->tab_strip_model()->GetWebContentsAt(singleton_index);
  } else if (params->disposition == WindowOpenDisposition::SWITCH_TO_TAB) {
    // The user is trying to open a tab that no longer exists. If we open a new
    // tab, it could leave orphaned NTPs around, but always overwriting the
    // current tab could could clobber state that the user was trying to
    // preserve. Fallback to the behavior used for singletons: overwrite the
    // current tab if it's the NTP, otherwise open a new tab.
    params->disposition = WindowOpenDisposition::SINGLETON_TAB;
    ShowSingletonTabOverwritingNTP(params->browser, std::move(*params));
    return;
  }
#if defined(OS_CHROMEOS)
  if (source_browser) {
    // If OS Settings is accessed in any means other than explicitly typing the
    // URL into the URL bar, open OS Settings in its own standalone surface.
    if (chromeos::features::IsSplitSettingsEnabled() &&
        params->url.host() == chrome::kChromeUIOSSettingsHost &&
        !PageTransitionCoreTypeIs(params->transition,
                                  ui::PageTransition::PAGE_TRANSITION_TYPED)) {
      chrome::SettingsWindowManager* settings_window_manager =
          chrome::SettingsWindowManager::GetInstance();
      if (!settings_window_manager->IsSettingsBrowser(source_browser)) {
        settings_window_manager->ShowChromePageForProfile(
            GetSourceProfile(params), params->url);
        return;
      }
    }

    if (source_browser != params->browser) {
      // When the newly created browser was spawned by a browser which visits
      // another user's desktop, it should be shown on the same desktop as the
      // originating one. (This is part of the desktop separation per profile).
      auto* window_manager = MultiUserWindowManagerHelper::GetWindowManager();
      // Some unit tests have no client instantiated.
      if (window_manager) {
        aura::Window* src_window = source_browser->window()->GetNativeWindow();
        aura::Window* new_window = params->browser->window()->GetNativeWindow();
        const AccountId& src_account_id =
            window_manager->GetUserPresentingWindow(src_window);
        if (src_account_id !=
            window_manager->GetUserPresentingWindow(new_window)) {
          // Once the window gets presented, it should be shown on the same
          // desktop as the desktop of the creating browser. Note that this
          // command will not show the window if it wasn't shown yet by the
          // browser creation.
          window_manager->ShowWindowForUser(new_window, src_account_id);
        }
      }
    }
  }
#endif

  // Navigate() must not return early after this point.

  if (GetSourceProfile(params) != params->browser->profile()) {
    // A tab is being opened from a link from a different profile, we must reset
    // source information that may cause state to be shared.
    params->opener = nullptr;
    params->source_contents = nullptr;
    params->source_site_instance = nullptr;
    params->referrer = content::Referrer();
  }

  // Make sure the Browser is shown if params call for it.
  ScopedBrowserShower shower(params, &contents_to_navigate_or_insert);

  // Makes sure any WebContents created by this function is destroyed if
  // not properly added to a tab strip.
  std::unique_ptr<WebContents> contents_to_insert =
      std::move(params->contents_to_insert);

  // Some dispositions need coercion to base types.
  NormalizeDisposition(params);

  // If a new window has been created, it needs to be shown.
  if (params->window_action == NavigateParams::NO_ACTION &&
      source_browser != params->browser &&
      params->browser->tab_strip_model()->empty()) {
    params->window_action = NavigateParams::SHOW_WINDOW;
  }

  // If we create a popup window from a non user-gesture, don't activate it.
  if (params->window_action == NavigateParams::SHOW_WINDOW &&
      params->disposition == WindowOpenDisposition::NEW_POPUP &&
      params->user_gesture == false) {
    params->window_action = NavigateParams::SHOW_WINDOW_INACTIVE;
  }

  // Determine if the navigation was user initiated. If it was, we need to
  // inform the target WebContents, and we may need to update the UI.
  bool user_initiated =
      params->transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR ||
      ui::PageTransitionCoreTypeIs(params->transition,
                                   ui::PAGE_TRANSITION_TYPED) ||
      ui::PageTransitionCoreTypeIs(params->transition,
                                   ui::PAGE_TRANSITION_AUTO_BOOKMARK) ||
      ui::PageTransitionCoreTypeIs(params->transition,
                                   ui::PAGE_TRANSITION_GENERATED) ||
      ui::PageTransitionCoreTypeIs(params->transition,
                                   ui::PAGE_TRANSITION_AUTO_TOPLEVEL) ||
      ui::PageTransitionCoreTypeIs(params->transition,
                                   ui::PAGE_TRANSITION_RELOAD) ||
      ui::PageTransitionCoreTypeIs(params->transition,
                                   ui::PAGE_TRANSITION_KEYWORD);

  // Did we use a prerender?
  bool swapped_in_prerender = false;

  // If no target WebContents was specified (and we didn't seek and find a
  // singleton), we need to construct one if we are supposed to target a new
  // tab.
  if (!contents_to_navigate_or_insert) {
    DCHECK(!params->url.is_empty());
    if (params->disposition != WindowOpenDisposition::CURRENT_TAB) {
      contents_to_insert = CreateTargetContents(*params, params->url);
      contents_to_navigate_or_insert = contents_to_insert.get();
    } else {
      // ... otherwise if we're loading in the current tab, the target is the
      // same as the source.
      DCHECK(params->source_contents);
      contents_to_navigate_or_insert = params->source_contents;

      prerender::PrerenderManager::Params prerender_params(
          params, params->source_contents);

      // Prerender can only swap in CURRENT_TAB navigations; others have
      // different sessionStorage namespaces.
      swapped_in_prerender = SwapInPrerender(params->url, &prerender_params);
      if (swapped_in_prerender)
        contents_to_navigate_or_insert = prerender_params.replaced_contents;
    }

    if (!swapped_in_prerender) {
      // Try to handle non-navigational URLs that popup dialogs and such, these
      // should not actually navigate.
      if (!HandleNonNavigationAboutURL(params->url)) {
        // Perform the actual navigation, tracking whether it came from the
        // renderer.

        LoadURLInContents(contents_to_navigate_or_insert, params->url, params);
      }
    }
  } else {
    // |contents_to_navigate_or_insert| was specified non-NULL, and so we assume
    // it has already been navigated appropriately. We need to do nothing more
    // other than add it to the appropriate tabstrip.
  }

  // If the user navigated from the omnibox, and the selected tab is going to
  // lose focus, then make sure the focus for the source tab goes away from the
  // omnibox.
  if (params->source_contents &&
      (params->disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
       params->disposition == WindowOpenDisposition::NEW_WINDOW) &&
      (params->tabstrip_add_types & TabStripModel::ADD_INHERIT_OPENER))
    params->source_contents->Focus();

  if (params->source_contents == contents_to_navigate_or_insert ||
      (swapped_in_prerender &&
       params->disposition == WindowOpenDisposition::CURRENT_TAB)) {
    // The navigation occurred in the source tab.
    params->browser->UpdateUIForNavigationInTab(
        contents_to_navigate_or_insert, params->transition,
        params->window_action, user_initiated);
  } else if (singleton_index == -1) {
    // If some non-default value is set for the index, we should tell the
    // TabStripModel to respect it.
    if (params->tabstrip_index != -1)
      params->tabstrip_add_types |= TabStripModel::ADD_FORCE_INDEX;

    DCHECK(contents_to_insert);
    // The navigation should insert a new tab into the target Browser.
    params->browser->tab_strip_model()->AddWebContents(
        std::move(contents_to_insert), params->tabstrip_index,
        params->transition, params->tabstrip_add_types, params->group);
  }

  if (singleton_index >= 0) {
    // If switching browsers, make sure it is shown.
    if (params->disposition == WindowOpenDisposition::SWITCH_TO_TAB &&
        params->browser != source_browser)
      params->window_action = NavigateParams::SHOW_WINDOW;

    if (contents_to_navigate_or_insert->IsCrashed()) {
      contents_to_navigate_or_insert->GetController().Reload(
          content::ReloadType::NORMAL, true);
    } else if (params->path_behavior == NavigateParams::IGNORE_AND_NAVIGATE &&
               contents_to_navigate_or_insert->GetURL() != params->url) {
      LoadURLInContents(contents_to_navigate_or_insert, params->url, params);
    }

    // If the singleton tab isn't already selected, select it.
    if (params->source_contents != contents_to_navigate_or_insert) {
      // Use the index before the potential close below, because it could
      // make the index refer to a different tab.
      auto gesture_type = user_initiated ? TabStripModel::GestureType::kOther
                                         : TabStripModel::GestureType::kNone;
      bool should_close_this_tab = false;
      if (params->disposition == WindowOpenDisposition::SWITCH_TO_TAB) {
        // Close orphaned NTP (and the like) with no history when the user
        // switches away from them.
        if (params->source_contents->GetController().CanGoBack() ||
            (params->source_contents->GetLastCommittedURL().spec() !=
                 chrome::kChromeUINewTabURL &&
             params->source_contents->GetLastCommittedURL().spec() !=
                 chrome::kChromeSearchLocalNtpUrl &&
             params->source_contents->GetLastCommittedURL().spec() !=
                 url::kAboutBlankURL)) {
          // Blur location bar before state save in ActivateTabAt() below.
          params->source_contents->Focus();
        } else {
          should_close_this_tab = true;
        }
      }
      params->browser->tab_strip_model()->ActivateTabAt(singleton_index,
                                                        {gesture_type});
      // Close tab after switch so index remains correct.
      if (should_close_this_tab)
        params->source_contents->Close();
    }
  }

  params->navigated_or_inserted_contents = contents_to_navigate_or_insert;
}

bool IsHostAllowedInIncognito(const GURL& url) {
  std::string scheme = url.scheme();
  base::StringPiece host = url.host_piece();
  if (scheme == chrome::kChromeSearchScheme) {
    return host != chrome::kChromeUIThumbnailHost &&
           host != chrome::kChromeUIThumbnailHost2 &&
           host != chrome::kChromeUIThumbnailListHost &&
           host != chrome::kChromeUISuggestionsHost;
  }

  if (scheme != content::kChromeUIScheme)
    return true;

  if (host == chrome::kChromeUIChromeSigninHost) {
#if defined(OS_WIN)
    // Allow incognito mode for the chrome-signin url if we only want to
    // retrieve the login scope token without touching any profiles. This
    // option is only available on Windows for use with Google Credential
    // Provider for Windows.
    return signin::GetSigninReasonForEmbeddedPromoURL(url) ==
           signin_metrics::Reason::REASON_FETCH_LST_ONLY;
#else
    return false;
#endif  // defined(OS_WIN)
  }

  // Most URLs are allowed in incognito; the following are exceptions.
  // chrome://extensions is on the list because it redirects to
  // chrome://settings.
  return host != chrome::kChromeUIAppLauncherPageHost &&
         host != chrome::kChromeUISettingsHost &&
#if defined(OS_CHROMEOS)
         host != chrome::kChromeUIOSSettingsHost &&
#endif
         host != chrome::kChromeUIHelpHost &&
         host != chrome::kChromeUIHistoryHost &&
         host != chrome::kChromeUIExtensionsHost &&
         host != chrome::kChromeUIBookmarksHost &&
         host != chrome::kChromeUIThumbnailHost &&
         host != chrome::kChromeUIThumbnailHost2 &&
         host != chrome::kChromeUIThumbnailListHost &&
         host != chrome::kChromeUISuggestionsHost &&
         host != chrome::kChromeUIDevicesHost;
}

bool IsURLAllowedInIncognito(const GURL& url,
                             content::BrowserContext* browser_context) {
  if (url.scheme() == content::kViewSourceScheme) {
    // A view-source URL is allowed in incognito mode only if the URL itself
    // is allowed in incognito mode. Remove the "view-source:" from the start
    // of the URL and validate the rest.
    std::string stripped_spec = url.spec();
    DCHECK_GT(stripped_spec.size(), strlen(content::kViewSourceScheme));
    stripped_spec.erase(0, strlen(content::kViewSourceScheme) + 1);
    GURL stripped_url(stripped_spec);
    return stripped_url.is_valid() &&
           IsURLAllowedInIncognito(stripped_url, browser_context);
  }

  return IsHostAllowedInIncognito(url);
}
