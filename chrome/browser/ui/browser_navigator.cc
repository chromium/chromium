// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/web_contents_app_id_utils.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
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
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/url_constants.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "components/account_id/account_id.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/lacros_url_handling.h"
#include "chromeos/crosapi/cpp/gurl_os_handler_utils.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#endif

using content::GlobalRequestID;
using content::NavigationController;
using content::WebContents;
using WebExposedIsolationLevel =
    content::RenderFrameHost::WebExposedIsolationLevel;

class BrowserNavigatorWebContentsAdoption {
 public:
  static void AttachTabHelpers(content::WebContents* contents) {
    TabHelpers::AttachTabHelpers(contents);

    // Make the tab show up in the task manager.
    task_manager::WebContentsTags::CreateForTabContents(contents);
  }
};

namespace {

// Returns true if |params.browser| exists and can open a new tab for
// |params.url|. Not all browsers support multiple tabs, such as app frames and
// popups. TYPE_APP will open a new tab if the browser was launched from a
// template, otherwise only if the URL is within the app scope.
bool WindowCanOpenTabs(const NavigateParams& params) {
  if (!params.browser) {
    return false;
  }

  // If the browser is created from a template, we do not need to check if the
  // url is in the app scope since we know it was saved directly from the app.
  if (params.browser->creation_source() !=
          Browser::CreationSource::kDeskTemplate &&
      params.browser->app_controller() &&
      !params.browser->app_controller()->IsUrlInAppScope(params.url)) {
    return false;
  }

  return params.browser->CanSupportWindowFeature(Browser::FEATURE_TABSTRIP) ||
         params.browser->tab_strip_model()->empty();
}

// Finds an existing Browser compatible with |profile|, making a new one if no
// such Browser is located.
Browser* GetOrCreateBrowser(Profile* profile, bool user_gesture) {
  Browser* browser = chrome::FindTabbedBrowser(profile, false);

  if (!browser && Browser::GetCreationStatusForProfile(profile) ==
                      Browser::CreationStatus::kOk) {
    browser = Browser::Create(Browser::CreateParams(profile, user_gesture));
  }
  return browser;
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
                     IncognitoModePrefs::Availability::kForced) {
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

  if (params.open_pwa_window_if_possible) {
    absl::optional<web_app::AppId> app_id =
        web_app::FindInstalledAppWithUrlInScope(profile, params.url,
                                                /*window_only=*/true);
    if (!app_id && params.force_open_pwa_window) {
      // In theory |force_open_pwa_window| should only be set if we know a
      // matching PWA is installed. However, we can reach here if
      // WebAppRegistrary hasn't finished loading yet, which can happen if
      // Chrome is launched with the URL of an isolated app as an argument.
      // This isn't a supported way to launch isolated apps, so we can cancel
      // the navigation, but if we want to support it in the future we'll need
      // to block until WebAppRegistrar is loaded.
      return {nullptr, -1};
    }
    if (app_id) {
      // Reuse the existing browser for in-app same window navigations.
      bool navigating_same_app =
          params.browser &&
          web_app::AppBrowserController::IsForWebApp(params.browser, *app_id);
      if (navigating_same_app &&
          params.disposition == WindowOpenDisposition::CURRENT_TAB) {
        return {params.browser, -1};
      }
      // App popups are handled in the switch statement below.
      if (params.disposition != WindowOpenDisposition::NEW_POPUP) {
        // Open a new app window.
        std::string app_name =
            web_app::GenerateApplicationNameFromAppId(*app_id);
        Browser* browser = nullptr;
        if (Browser::GetCreationStatusForProfile(profile) ==
            Browser::CreationStatus::kOk) {
          browser = Browser::Create(Browser::CreateParams::CreateForApp(
              app_name,
              true,  // trusted_source. Installed PWAs are considered trusted.
              params.window_bounds, profile, params.user_gesture));
        }
        return {browser, -1};
      }
    }
  }

  switch (params.disposition) {
    case WindowOpenDisposition::SWITCH_TO_TAB:
#if !BUILDFLAG(IS_ANDROID)
    {
      std::pair<Browser*, int> index =
          GetIndexAndBrowserOfExistingTab(profile, params);
      if (index.first)
        return index;
    }
#endif
      [[fallthrough]];
    case WindowOpenDisposition::CURRENT_TAB:
      if (params.browser)
        return {params.browser, -1};
      // Find a compatible window and re-execute this command in it. Otherwise
      // re-run with NEW_WINDOW.
      return {GetOrCreateBrowser(profile, params.user_gesture), -1};
    case WindowOpenDisposition::SINGLETON_TAB: {
      // If we have a browser window, check it first.
      if (params.browser) {
        int index = GetIndexOfExistingTab(params.browser, params);
        if (index >= 0)
          return {params.browser, index};
      }
      // If we don't have a a window, or if this window can't open tabs, then
      // it would load in a random window, potentially opening a second copy.
      // Instead, make an extra effort to see if there's an already open copy.
      if (!WindowCanOpenTabs(params)) {
        std::pair<Browser*, int> index =
            GetIndexAndBrowserOfExistingTab(profile, params);
        if (index.first)
          return index;
      }
    }
      [[fallthrough]];
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      // See if we can open the tab in the window this navigator is bound to.
      if (WindowCanOpenTabs(params))
        return {params.browser, -1};

      // Find a compatible window and re-execute this command in it. Otherwise
      // re-run with NEW_WINDOW.
      return {GetOrCreateBrowser(profile, params.user_gesture), -1};
    case WindowOpenDisposition::NEW_PICTURE_IN_PICTURE:
#if !BUILDFLAG(IS_ANDROID)
      // Picture in picture windows may not be opened by other picture in
      // picture windows.
      if (params.browser->is_type_picture_in_picture())
        return {nullptr, -1};

      {
        Browser::CreateParams browser_params(Browser::TYPE_PICTURE_IN_PICTURE,
                                             profile, params.user_gesture);
        browser_params.trusted_source = params.trusted_source;
        if (params.contents_to_insert) {
          auto pip_options =
              params.contents_to_insert->GetPictureInPictureOptions();
          if (!pip_options.has_value()) {
            return {nullptr, -1};
          }
          browser_params.initial_bounds = PictureInPictureWindowManager::
              CalculateInitialPictureInPictureWindowBounds(
                  *pip_options,
                  display::Screen::GetScreen()->GetDisplayForNewWindows());
          browser_params.initial_aspect_ratio =
              pip_options->initial_aspect_ratio > 0.0
                  ? pip_options->initial_aspect_ratio
                  : 1.0;
          browser_params.lock_aspect_ratio = pip_options->lock_aspect_ratio;
          browser_params.omit_from_session_restore = true;
        }

        return {Browser::Create(browser_params), -1};
      }
#else   // !IS_ANDROID
      // For TYPE_PICTURE_IN_PICTURE
      NOTIMPLEMENTED_LOG_ONCE();
      return {nullptr, -1};
#endif  // !IS_ANDROID

    case WindowOpenDisposition::NEW_POPUP: {
      // Make a new popup window.
      // Coerce app-style if |source| represents an app.
      std::string app_name;
      if (!params.app_id.empty()) {
        app_name = web_app::GenerateApplicationNameFromAppId(params.app_id);
      } else if (params.browser && !params.browser->app_name().empty()) {
        app_name = params.browser->app_name();
      }
      if (Browser::GetCreationStatusForProfile(profile) !=
          Browser::CreationStatus::kOk) {
        return {nullptr, -1};
      }
      if (app_name.empty()) {
        Browser::CreateParams browser_params(Browser::TYPE_POPUP, profile,
                                             params.user_gesture);
        browser_params.trusted_source = params.trusted_source;
        browser_params.initial_bounds = params.window_bounds;
        return {Browser::Create(browser_params), -1};
      }
      return {Browser::Create(Browser::CreateParams::CreateForAppPopup(
                  app_name, params.trusted_source, params.window_bounds,
                  profile, params.user_gesture)),
              -1};
    }
    case WindowOpenDisposition::NEW_WINDOW: {
      // Make a new normal browser window.
      Browser* browser = nullptr;
      if (Browser::GetCreationStatusForProfile(profile) ==
          Browser::CreationStatus::kOk) {
        browser = Browser::Create(
            Browser::CreateParams(profile, params.user_gesture));
      }
      return {browser, -1};
    }
    case WindowOpenDisposition::OFF_THE_RECORD:
      // Make or find an incognito window.
      return {GetOrCreateBrowser(
                  profile->GetPrimaryOTRProfile(/*create_if_needed=*/true),
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
      params->tabstrip_add_types &= ~AddTabTypes::ADD_ACTIVE;
      break;

    case WindowOpenDisposition::NEW_PICTURE_IN_PICTURE:
      // Always show a new picture in picture window.
      params->window_action = NavigateParams::SHOW_WINDOW_INACTIVE;
      break;

    case WindowOpenDisposition::NEW_WINDOW:
    case WindowOpenDisposition::NEW_POPUP: {
      // Code that wants to open a new window typically expects it to be shown
      // automatically.
      if (params->window_action == NavigateParams::NO_ACTION)
        params->window_action = NavigateParams::SHOW_WINDOW;
      [[fallthrough]];
    }
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
    case WindowOpenDisposition::SINGLETON_TAB:
      params->tabstrip_add_types |= AddTabTypes::ADD_ACTIVE;
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

base::WeakPtr<content::NavigationHandle> LoadURLInContents(
    WebContents* target_contents,
    const GURL& url,
    NavigateParams* params) {
  NavigationController::LoadURLParams load_url_params(url);
  load_url_params.initiator_frame_token = params->initiator_frame_token;
  load_url_params.initiator_process_id = params->initiator_process_id;
  load_url_params.initiator_origin = params->initiator_origin;
  load_url_params.initiator_base_url = params->initiator_base_url;
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
  load_url_params.impression = params->impression;

  // |frame_tree_node_id| is kNoFrameTreeNodeId for main frame navigations.
  if (params->frame_tree_node_id ==
      content::RenderFrameHost::kNoFrameTreeNodeId) {
    load_url_params.navigation_ui_data =
        ChromeNavigationUIData::CreateForMainFrameNavigation(
            target_contents, params->disposition,
            params->is_using_https_as_default_scheme);
  }

  if (params->post_data) {
    load_url_params.load_type = NavigationController::LOAD_TYPE_HTTP_POST;
    load_url_params.post_data = params->post_data;
  }

  return target_contents->GetController().LoadURLWithParams(load_url_params);
}

// This class makes sure the Browser object held in |params| is made visible
// by the time it goes out of scope, provided |params| wants it to be shown.
class ScopedBrowserShower {
 public:
  explicit ScopedBrowserShower(NavigateParams* params,
                               content::WebContents** contents)
      : params_(params), contents_(contents) {}

  ScopedBrowserShower(const ScopedBrowserShower&) = delete;
  ScopedBrowserShower& operator=(const ScopedBrowserShower&) = delete;

  ~ScopedBrowserShower() {
    BrowserWindow* window = params_->browser->window();
    if (params_->window_action == NavigateParams::SHOW_WINDOW_INACTIVE) {
      window->ShowInactive();
    } else if (params_->window_action == NavigateParams::SHOW_WINDOW) {
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
  raw_ptr<NavigateParams> params_;
  raw_ptr<content::WebContents*> contents_;
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

  create_params.opened_by_another_window = params.opened_by_another_window;

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
  apps::SetAppIdForWebContents(params.browser->profile(), target_contents.get(),
                               params.app_id);

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  if (params.is_captive_portal_popup) {
    DCHECK_EQ(WindowOpenDisposition::NEW_POPUP, params.disposition);
    captive_portal::CaptivePortalTabHelper::FromWebContents(
        target_contents.get())
        ->set_is_captive_portal_window();
  }
#endif

  return target_contents;
}

}  // namespace

base::WeakPtr<content::NavigationHandle> Navigate(NavigateParams* params) {
  TRACE_EVENT1("navigation", "chrome::Navigate", "disposition",
               params->disposition);
  Browser* source_browser = params->browser;
  if (source_browser)
    params->initiating_profile = source_browser->profile();
  DCHECK(params->initiating_profile);

  if (source_browser &&
      platform_util::IsBrowserLockedFullscreen(source_browser)) {
    // Block any navigation requests in locked fullscreen mode.
    return nullptr;
  }

  // Open System Apps in their standalone window if necessary.
  // TODO(crbug.com/1096345): Remove this code after we integrate with intent
  // handling.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const absl::optional<ash::SystemWebAppType> capturing_system_app_type =
      ash::GetCapturingSystemAppForURL(params->initiating_profile, params->url);
  if (capturing_system_app_type &&
      (!params->browser ||
       !ash::IsBrowserForSystemWebApp(params->browser,
                                      capturing_system_app_type.value()))) {
    ash::SystemAppLaunchParams swa_params;
    swa_params.url = params->url;
    ash::LaunchSystemWebAppAsync(params->initiating_profile,
                                 capturing_system_app_type.value(), swa_params);

    // It's okay to early return here, because LaunchSystemWebAppAsync uses a
    // different logic to choose (and create if necessary) a browser window for
    // system apps.
    //
    // It's okay to skip the checks and cleanups below. The link captured system
    // app will either open in its own browser window, or navigate an existing
    // browser window exclusively used by this app. For the initiating browser,
    // the navigation should appear to be cancelled.
    return nullptr;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
  // Force isolated PWAs to open in an app window.
  params->force_open_pwa_window =
      content::SiteIsolationPolicy::ShouldUrlUseApplicationIsolationLevel(
          params->initiating_profile, params->url);
  params->open_pwa_window_if_possible |= params->force_open_pwa_window;
#endif

  if (!AdjustNavigateParamsForURL(params))
    return nullptr;

  // Trying to open a background tab when in an app browser results in
  // focusing a regular browser window and opening a tab in the background
  // of that window. Change the disposition to NEW_FOREGROUND_TAB so that
  // the new tab is focused.
  if (source_browser && source_browser->is_type_app() &&
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
    return nullptr;
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
    ShowSingletonTabOverwritingNTP(params->browser, params);
    return nullptr;
  }
  if (params->force_open_pwa_window) {
    CHECK(web_app::AppBrowserController::IsWebApp(params->browser));
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (source_browser && source_browser != params->browser) {
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
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  const GURL& source_url =
      params->source_contents ? params->source_contents->GetURL() : GURL();
  if (lacros_url_handling::IsNavigationInterceptable(*params, source_url) &&
      lacros_url_handling::MaybeInterceptNavigation(params->url)) {
    return nullptr;
  }
  // If Lacros comes here with an internal os:// redirect scheme to Ash, and Ash
  // does not accept the URL, we convert it into a blocked url instead.
  if (crosapi::gurl_os_handler_utils::IsAshOsUrl(params->url)) {
    params->url = GURL(content::kBlockedURL);
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
      !ui::PageTransitionIsWebTriggerable(params->transition);

  base::WeakPtr<content::NavigationHandle> navigation_handle;

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
    }

    // Try to handle non-navigational URLs that popup dialogs and such, these
    // should not actually navigate.
    if (!HandleNonNavigationAboutURL(params->url)) {
      // Perform the actual navigation, tracking whether it came from the
      // renderer.

      navigation_handle = LoadURLInContents(contents_to_navigate_or_insert,
                                            params->url, params);
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
      (params->tabstrip_add_types & AddTabTypes::ADD_INHERIT_OPENER))
    params->source_contents->Focus();

  if (params->source_contents == contents_to_navigate_or_insert) {
    // The navigation occurred in the source tab.
    params->browser->UpdateUIForNavigationInTab(
        contents_to_navigate_or_insert, params->transition,
        params->window_action, user_initiated);
  } else if (singleton_index == -1) {
    if (source_browser != params->browser)
      params->tabstrip_index = params->browser->tab_strip_model()->count();

    // If some non-default value is set for the index, we should tell the
    // TabStripModel to respect it.
    if (params->tabstrip_index != -1)
      params->tabstrip_add_types |= AddTabTypes::ADD_FORCE_INDEX;

    // Maybe notify that an open operation has been done from a gesture.
    // TODO(crbug.com/1129028): preferably pipe this information through the
    // TabStripModel instead. See bug for deeper discussion.
    if (params->user_gesture && source_browser == params->browser)
      params->browser->window()->LinkOpeningFromGesture(params->disposition);

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
      navigation_handle = LoadURLInContents(contents_to_navigate_or_insert,
                                            params->url, params);
    }

    // If the singleton tab isn't already selected, select it.
    if (params->source_contents != contents_to_navigate_or_insert) {
      // Use the index before the potential close below, because it could
      // make the index refer to a different tab.
      auto gesture_type = user_initiated
                              ? TabStripUserGestureDetails::GestureType::kOther
                              : TabStripUserGestureDetails::GestureType::kNone;
      bool should_close_this_tab = false;
      if (params->disposition == WindowOpenDisposition::SWITCH_TO_TAB) {
        // Close orphaned NTP (and the like) with no history when the user
        // switches away from them.
        if (params->source_contents) {
          if (params->source_contents->GetController().CanGoBack() ||
              (params->source_contents->GetLastCommittedURL().spec() !=
                   chrome::kChromeUINewTabURL &&
               params->source_contents->GetLastCommittedURL().spec() !=
                   url::kAboutBlankURL)) {
            // Blur location bar before state save in ActivateTabAt() below.
            params->source_contents->Focus();
          } else {
            should_close_this_tab = true;
          }
        }
      }
      params->browser->tab_strip_model()->ActivateTabAt(
          singleton_index, TabStripUserGestureDetails(gesture_type));
      // Close tab after switch so index remains correct.
      if (should_close_this_tab)
        params->source_contents->Close();
    }
  }

#if !BUILDFLAG(IS_ANDROID)
  // If this is a Picture in Picture window, then notify the pip manager about
  // it. This enables the opener and pip window to stay connected, so that (for
  // example), the pip window does not outlive the opener.
  if (params->disposition == WindowOpenDisposition::NEW_PICTURE_IN_PICTURE) {
    PictureInPictureWindowManager::GetInstance()->EnterDocumentPictureInPicture(
        params->source_contents, contents_to_navigate_or_insert);
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  params->navigated_or_inserted_contents = contents_to_navigate_or_insert;
  return navigation_handle;
}

bool IsHostAllowedInIncognito(const GURL& url) {
  std::string scheme = url.scheme();
  base::StringPiece host = url.host_piece();
  if (scheme != content::kChromeUIScheme)
    return true;

  if (host == chrome::kChromeUIChromeSigninHost) {
#if BUILDFLAG(IS_WIN)
    // Allow incognito mode for the chrome-signin url if we only want to
    // retrieve the login scope token without touching any profiles. This
    // option is only available on Windows for use with Google Credential
    // Provider for Windows.
    return signin::GetSigninReasonForEmbeddedPromoURL(url) ==
           signin_metrics::Reason::kFetchLstOnly;
#else
    return false;
#endif  // BUILDFLAG(IS_WIN)
  }

  // Most URLs are allowed in incognito; the following are exceptions.
  // chrome://extensions is on the list because it redirects to
  // chrome://settings.
  return host != chrome::kChromeUIAppLauncherPageHost &&
         host != chrome::kChromeUISettingsHost &&
#if BUILDFLAG(IS_CHROMEOS_ASH)
         host != chrome::kChromeUIOSSettingsHost &&
#endif
         host != chrome::kChromeUIHelpHost &&
         host != chrome::kChromeUIHistoryHost &&
         host != chrome::kChromeUIExtensionsHost &&
         host != chrome::kChromeUIBookmarksHost &&
         host != password_manager::kChromeUIPasswordManagerHost;
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
    if (stripped_url.is_empty())
      return true;
    return stripped_url.is_valid() &&
           IsURLAllowedInIncognito(stripped_url, browser_context);
  }

  return IsHostAllowedInIncognito(url);
}
