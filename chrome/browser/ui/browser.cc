// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/process/process_info.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/background/background_contents.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/content_settings/sound_content_setting_observer.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/devtools/devtools_toggle_action.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/repost_form_warning_controller.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/app_session_service.h"
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_lookup.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/blocked_content/chrome_popup_navigation_delegate.h"
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/breadcrumb_manager_browser_agent.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_location_bar_model_delegate.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tab_menu_model_delegate.h"
#include "chrome/browser/ui/browser_tab_strip_model_delegate.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_ui_prefs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/pointer_lock_controller.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/overscroll_pref_manager.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/signin/cookie_clear_on_exit_migration_notice.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/sync/browser_synced_window_delegate.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/unload_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/blocked_content/list_item_position.h"
#include "components/blocked_content/popup_blocker.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/blocked_content/popup_tracker.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/breadcrumbs/core/breadcrumbs_status.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/custom_handlers/register_protocol_handler_permission_request.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "components/permissions/permission_request_manager.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/search/search.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/translate/core/browser/language_state.h"
#include "components/user_manager/user_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_exposed_isolation_level.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/profiling.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/common/window_container_type.mojom-shared.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "net/base/filename_util.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"
#include "third_party/blink/public/mojom/frame/blocked_navigation_types.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(IS_WIN)
// windows.h must be included before shellapi.h
#include <windows.h>

#include <shellapi.h>

#include "chrome/browser/ui/view_ids.h"
#include "ui/base/win/shell.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "components/session_manager/core/session_manager.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_browser_window_helper.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/printing/browser/print_composite_client.h"
#endif

#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
#include "components/paint_preview/browser/paint_preview_client.h"  // nogncheck
#endif

#if BUILDFLAG(IS_MAC)
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#endif  // BUILDFLAG(IS_MAC)

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/preloading/preview/preview_manager.h"
#endif

using base::UserMetricsAction;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::RenderWidgetHostView;
using content::SiteInstance;
using content::WebContents;
using custom_handlers::ProtocolHandler;
using extensions::Extension;
using input::NativeWebKeyboardEvent;
using ui::WebDialogDelegate;
using web_modal::WebContentsModalDialogManager;

///////////////////////////////////////////////////////////////////////////////

namespace {

// How long we wait before updating the browser chrome while loading a page.
constexpr base::TimeDelta kUIUpdateCoalescingTime = base::Milliseconds(200);

BrowserWindow* CreateBrowserWindow(std::unique_ptr<Browser> browser,
                                   bool user_gesture,
                                   bool in_tab_dragging) {
  return BrowserWindow::CreateBrowserWindow(std::move(browser), user_gesture,
                                            in_tab_dragging);
}

const extensions::Extension* GetExtensionForOrigin(
    Profile* profile,
    const GURL& security_origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!security_origin.SchemeIs(extensions::kExtensionScheme)) {
    return nullptr;
  }

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          security_origin.host());
  DCHECK(extension);
  return extension;
#else
  return nullptr;
#endif
}

bool IsOnKioskSplashScreen() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  if (!session_manager)
    return false;
  // We have to check this way because of CHECK() in UserManager::Get().
  if (!user_manager::UserManager::IsInitialized())
    return false;
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!user_manager->IsLoggedInAsAnyKioskApp())
    return false;
  if (session_manager->session_state() !=
      session_manager::SessionState::LOGIN_PRIMARY)
    return false;
  return true;
#else
  return false;
#endif
}

// Returns a pair [last_window, last_window_for_profile] indicating if `browser`
// is the only browser in total and for this profile.
// Ignores browsers that are in the process of closing.
std::pair<bool, bool> IsLastWindow(const Browser& browser) {
  bool last_window = true;
  bool last_window_for_profile = true;
  for (Browser* other_browser : *BrowserList::GetInstance()) {
    // Don't count this browser window or any other in the process of closing.
    // Window closing may be delayed, and windows that are in the process of
    // closing don't count against our totals.
    if (other_browser == &browser ||
        other_browser->IsAttemptingToCloseBrowser()) {
      continue;
    }

    last_window = false;

    if (other_browser->profile() == browser.profile()) {
      last_window_for_profile = false;
      break;
    }
  }

  return {last_window, last_window_for_profile};
}

// Returns whether the cookie migration notice should be shown: the migration
// is not complete, and this is the last browser window open for this profile.
bool ShouldShowCookieMigrationNoticeForBrowser(const Browser& browser) {
  if (!CanShowCookieClearOnExitMigrationNotice(browser)) {
    return false;
  }

  auto [last_window, last_window_for_profile] = IsLastWindow(browser);
  return last_window_for_profile;
}

void UpdateTabGroupSessionMetadata(Browser* browser,
                                   const tab_groups::TabGroupId& group_id,
                                   std::optional<std::string> saved_group_id) {
  SessionService* const session_service =
      SessionServiceFactory::GetForProfile(browser->profile());
  if (!session_service) {
    return;
  }

  const tab_groups::TabGroupVisualData* visual_data =
      browser->tab_strip_model()
          ->group_model()
          ->GetTabGroup(group_id)
          ->visual_data();

  session_service->SetTabGroupMetadata(browser->session_id(), group_id,
                                       visual_data, std::move(saved_group_id));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Browser, CreateParams:

Browser::CreateParams::CreateParams(Profile* profile, bool user_gesture)
    : CreateParams(TYPE_NORMAL, profile, user_gesture) {}

Browser::CreateParams::CreateParams(Type type,
                                    Profile* profile,
                                    bool user_gesture)
    : type(type), profile(profile), user_gesture(user_gesture) {}

Browser::CreateParams::CreateParams(const CreateParams& other) = default;

Browser::CreateParams& Browser::CreateParams::operator=(
    const CreateParams& other) = default;

Browser::CreateParams::~CreateParams() = default;

// static
Browser::CreateParams Browser::CreateParams::CreateForAppBase(
    bool is_popup,
    const std::string& app_name,
    bool trusted_source,
    const gfx::Rect& window_bounds,
    Profile* profile,
    bool user_gesture) {
  DCHECK(!app_name.empty());

  CreateParams params(is_popup ? Type::TYPE_APP_POPUP : Type::TYPE_APP, profile,
                      user_gesture);
  params.app_name = app_name;
  params.trusted_source = trusted_source;
  params.initial_bounds = window_bounds;
  params.are_tab_groups_enabled = false;

  return params;
}

// static
Browser::CreateParams Browser::CreateParams::CreateForApp(
    const std::string& app_name,
    bool trusted_source,
    const gfx::Rect& window_bounds,
    Profile* profile,
    bool user_gesture) {
  return CreateForAppBase(false, app_name, trusted_source, window_bounds,
                          profile, user_gesture);
}

// static
Browser::CreateParams Browser::CreateParams::CreateForAppPopup(
    const std::string& app_name,
    bool trusted_source,
    const gfx::Rect& window_bounds,
    Profile* profile,
    bool user_gesture) {
  return CreateForAppBase(true, app_name, trusted_source, window_bounds,
                          profile, user_gesture);
}

// static
Browser::CreateParams Browser::CreateParams::CreateForPictureInPicture(
    const std::string& app_name,
    bool trusted_source,
    Profile* profile,
    bool user_gesture) {
  Browser::CreateParams browser_params(Browser::TYPE_PICTURE_IN_PICTURE,
                                       profile, user_gesture);
  browser_params.app_name = app_name;
  browser_params.trusted_source = trusted_source;
  return browser_params;
}

// static
Browser::CreateParams Browser::CreateParams::CreateForDevTools(
    Profile* profile) {
  CreateParams params(TYPE_DEVTOOLS, profile, true);
  params.app_name = DevToolsWindow::kDevToolsApp;
  params.trusted_source = true;
  return params;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Constructors, Creation, Showing:

// static
Browser::CreationStatus Browser::GetCreationStatusForProfile(Profile* profile) {
  if (!g_browser_process || g_browser_process->IsShuttingDown())
    return CreationStatus::kErrorNoProcess;

  if (!IncognitoModePrefs::CanOpenBrowser(profile) ||
      (profile->IsGuestSession() && !profile->IsOffTheRecord()) ||
      !profile->AllowsBrowserWindows() ||
      IsProfileDirectoryMarkedForDeletion(profile->GetPath())) {
    return CreationStatus::kErrorProfileUnsuitable;
  }

  if (IsOnKioskSplashScreen())
    return CreationStatus::kErrorLoadingKiosk;

  return CreationStatus::kOk;
}

// static
Browser* Browser::Create(const CreateParams& params) {
  // If this is failing, a caller is trying to create a browser when creation is
  // not possible, e.g. using the wrong profile or during shutdown. The caller
  // should handle this; see e.g. crbug.com/1141608 and crbug.com/1261628.
  CHECK_EQ(CreationStatus::kOk, GetCreationStatusForProfile(params.profile));
  return new Browser(params);
}

Browser::Browser(const CreateParams& params)
    : create_params_(params),
      type_(params.type),
      profile_(params.profile),
      window_(nullptr),
      tab_strip_model_delegate_(
          std::make_unique<chrome::BrowserTabStripModelDelegate>(this)),
      tab_strip_model_(std::make_unique<TabStripModel>(
          tab_strip_model_delegate_.get(),
          params.profile,
          params.are_tab_groups_enabled ? TabGroupModelFactory::GetInstance()
                                        : nullptr)),
      tab_menu_model_delegate_(
          std::make_unique<chrome::BrowserTabMenuModelDelegate>(this)),
      app_name_(params.app_name),
      is_trusted_source_(params.trusted_source),
      session_id_(SessionID::NewUnique()),
      omit_from_session_restore_(params.omit_from_session_restore),
      should_trigger_session_restore_(params.should_trigger_session_restore),
      cancel_download_confirmation_state_(NOT_PROMPTED),
      override_bounds_(params.initial_bounds),
      initial_show_state_(params.initial_show_state),
      initial_workspace_(params.initial_workspace),
      initial_visible_on_all_workspaces_state_(
          params.initial_visible_on_all_workspaces_state),
      creation_source_(params.creation_source),
      unload_controller_(this),
      content_setting_bubble_model_delegate_(
          new BrowserContentSettingBubbleModelDelegate(this)),
      location_bar_model_delegate_(new BrowserLocationBarModelDelegate(this)),
      location_bar_model_(std::make_unique<LocationBarModelImpl>(
          location_bar_model_delegate_.get(),
          content::kMaxURLDisplayChars)),
      live_tab_context_(new BrowserLiveTabContext(this)),
      synced_window_delegate_(new BrowserSyncedWindowDelegate(this)),
      app_controller_(web_app::MaybeCreateAppBrowserController(this)),
      bookmark_bar_state_(BookmarkBar::HIDDEN),
      browser_actions_(new BrowserActions(*this)),
      command_controller_(new chrome::BrowserCommandController(this)),
      tab_group_deletion_dialog_controller_(
          std::make_unique<tab_groups::DeletionDialogController>(this)),
      window_has_shown_(false),
      user_title_(params.user_title),
      signin_view_controller_(this),
      breadcrumb_manager_browser_agent_(
          breadcrumbs::IsEnabled(g_browser_process->local_state())
              ? std::make_unique<BreadcrumbManagerBrowserAgent>(this)
              : nullptr)
#if BUILDFLAG(ENABLE_EXTENSIONS)
      ,
      extension_browser_window_helper_(
          std::make_unique<extensions::ExtensionBrowserWindowHelper>(this))
#endif
#if defined(USE_AURA)
      ,
      overscroll_pref_manager_(std::make_unique<OverscrollPrefManager>(this))
#endif
{
  browser_actions_->InitializeBrowserActions();

  if (!profile_->IsOffTheRecord()) {
    profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
        params.profile->GetOriginalProfile(),
        ProfileKeepAliveOrigin::kBrowserWindow);
  }

  tab_strip_model_->AddObserver(this);

  ThemeServiceFactory::GetForProfile(profile_)->AddObserver(this);

  profile_pref_registrar_.Init(profile_->GetPrefs());
  profile_pref_registrar_.Add(
      prefs::kDevToolsAvailability,
      base::BindRepeating(&Browser::OnDevToolsAvailabilityChanged,
                          base::Unretained(this)));
  profile_pref_registrar_.Add(
      bookmarks::prefs::kShowBookmarkBar,
      base::BindRepeating(&Browser::UpdateBookmarkBarState,
                          base::Unretained(this),
                          BOOKMARK_BAR_STATE_CHANGE_PREF_CHANGE));

  if (search::IsInstantExtendedAPIEnabled() && is_type_normal())
    instant_controller_ = std::make_unique<BrowserInstantController>(this);

  UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_INIT);

  ProfileMetrics::LogProfileLaunch(profile_);

  if (params.skip_window_init_for_testing)
    return;

  // BrowserWindowFeatures need to be initialized before browser window
  // creation, so that the features can be used in creating components
  // in browser window.
  features_ = BrowserWindowFeatures::CreateBrowserWindowFeatures();
  features_->Init(this);

  window_ = params.window ? params.window.get()
                          : CreateBrowserWindow(std::unique_ptr<Browser>(this),
                                                params.user_gesture,
                                                params.in_tab_dragging);

  if (app_controller_)
    app_controller_->UpdateCustomTabBarVisibility(false);

  // Create the extension window controller before sending notifications.
  extension_window_controller_ =
      std::make_unique<extensions::BrowserExtensionWindowController>(this);

  SessionServiceBase* service =
      GetAppropriateSessionServiceForSessionRestore(this);

  if (service)
    service->WindowOpened(this);

  exclusive_access_manager_ = std::make_unique<ExclusiveAccessManager>(
      window_->GetExclusiveAccessContext());

  if (window_->GetDownloadBubbleUIController()) {
    window_->GetDownloadBubbleUIController()
        ->GetDownloadDisplayController()
        ->ListenToFullScreenChanges();
  }

  // Initialize the browser features that rely on the browser window now that it
  // is initialized.
  features_->InitPostWindowConstruction(this);

  BrowserList::AddBrowser(this);
}

Browser::~Browser() {
  // Tear down `BrowserWindowFeatures` and `BrowserUserData`s now to avoid
  // exposing them to Browser in a partially-destroyed state. Eventually,
  // all BrowserUserData should be converted to features. Until then,
  // destroy `features_` because that's what breaks things the least :)
  features_.reset();
  ClearAllUserData();

  // Destroy the deletion dialog before profile resets.
  // (see https://crbug.com/357391254)
  tab_group_deletion_dialog_controller_.reset();

  // Stop observing notifications and destroy the tab monitor before continuing
  // with destruction. Profile destruction will unload extensions and reentrant
  // calls to Browser:: should be avoided while it is being torn down.
  ThemeServiceFactory::GetForProfile(profile_)->RemoveObserver(this);
  extension_browser_window_helper_.reset();

  // The tab strip should not have any tabs at this point.
  //
  // TODO(crbug.com/40887606): This DCHECK doesn't always pass.
  // TODO(crbug.com/40064092): convert this to CHECK.
  DCHECK(tab_strip_model_->empty());

  // Destroy the BrowserCommandController before removing the browser, so that
  // it doesn't act on any notifications that are sent as a result of removing
  // the browser.
  command_controller_.reset();

  // Remove listeners associated with browser actions so that
  // it doesn't act on any during browser destruction.
  browser_actions_->RemoveListeners();

  // Destroy ExclusiveAccessManager, which depends on `window_` which may be
  // destroyed by RemoveBrowser().
  exclusive_access_manager_.reset();
  BrowserList::RemoveBrowser(this);

  // If closing the window is going to trigger a shutdown, then we need to
  // schedule all active downloads to be cancelled. This needs to be after
  // removing |this| from BrowserList so that OkToClose...() can determine
  // whether there are any other windows open for the browser.
  int num_downloads;
  if (!browser_defaults::kBrowserAliveWithNoWindows &&
      OkToCloseWithInProgressDownloads(&num_downloads) ==
          DownloadCloseType::kBrowserShutdown) {
    DownloadCoreService::CancelAllDownloads(
        DownloadCoreService::CancelDownloadsTrigger::kShutdown);
  }

  SessionServiceBase* service = GetAppropriateSessionServiceForProfile(this);

  if (service)
    service->WindowClosed(session_id_);

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile());
  if (tab_restore_service)
    tab_restore_service->BrowserClosed(live_tab_context());

  profile_pref_registrar_.Reset();

  // Destroy BrowserExtensionWindowController before the incognito profile
  // is destroyed to make sure the chrome.windows.onRemoved event is sent.
  extension_window_controller_.reset();

  // Destroy BrowserInstantController before the incognito profile is destroyed,
  // because its destructor depends on this profile.
  instant_controller_.reset();

  // The system incognito profile should not try be destroyed using
  // ProfileDestroyer::DestroyProfileWhenAppropriate(). This profile can be
  // used, at least, by the user manager window. This window is not a browser,
  // therefore, BrowserList::IsOffTheRecordBrowserActiveForProfile(profile_)
  // returns false, while the user manager window is still opened.
  // This cannot be fixed in ProfileDestroyer::DestroyProfileWhenAppropriate(),
  // because the ProfileManager needs to be able to destroy all profiles when
  // it is destroyed. See crbug.com/527035
  //
  // Non-primary OffTheRecord profiles should not be destroyed directly by
  // Browser (e.g. for offscreen tabs, https://crbug.com/664351).
  //
  // TODO(crbug.com/40159237): Use ScopedProfileKeepAlive for Incognito too,
  // instead of separate logic for Incognito and regular profiles.
  if (profile_->IsIncognitoProfile() &&
      !BrowserList::IsOffTheRecordBrowserInUse(profile_) &&
      !profile_->IsSystemProfile()) {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    // The Printing Background Manager holds onto preview dialog WebContents
    // whose corresponding print jobs have not yet fully spooled. Make sure
    // these get destroyed before tearing down the incognito profile so that
    // their RenderFrameHosts can exit in time - see crbug.com/579155
    g_browser_process->background_printing_manager()
        ->DeletePreviewContentsForBrowserContext(profile_);
#endif
    // An incognito profile is no longer needed, this indirectly frees
    // its cache and cookies once it gets destroyed at the appropriate time.
    ProfileDestroyer::DestroyOTRProfileWhenAppropriate(profile_);
  }

  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

///////////////////////////////////////////////////////////////////////////////
// Getters & Setters

BrowserView& Browser::GetBrowserView() {
  return CHECK_DEREF(window_->AsBrowserView());
}

base::WeakPtr<Browser> Browser::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

base::WeakPtr<const Browser> Browser::AsWeakPtr() const {
  return weak_factory_.GetWeakPtr();
}

FindBarController* Browser::GetFindBarController() {
  if (!find_bar_controller_.get()) {
    find_bar_controller_ =
        std::make_unique<FindBarController>(window_->CreateFindBar());
    find_bar_controller_->find_bar()->SetFindBarController(
        find_bar_controller_.get());
    find_bar_controller_->ChangeWebContents(
        tab_strip_model_->GetActiveWebContents());
    find_bar_controller_->find_bar()->MoveWindowIfNecessary();
  }
  return find_bar_controller_.get();
}

bool Browser::HasFindBarController() const {
  return find_bar_controller_.get() != nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, State Storage and Retrieval for UI:

GURL Browser::GetNewTabURL() const {
  if (app_controller_)
    return app_controller_->GetAppNewTabUrl();
  return GURL(chrome::kChromeUINewTabURL);
}

gfx::Image Browser::GetCurrentPageIcon() const {
  WebContents* web_contents = tab_strip_model_->GetActiveWebContents();
  // |web_contents| can be NULL since GetCurrentPageIcon() is called by the
  // window during the window's creation (before tabs have been added).
  favicon::FaviconDriver* favicon_driver =
      web_contents
          ? favicon::ContentFaviconDriver::FromWebContents(web_contents)
          : nullptr;
  return favicon_driver ? favicon_driver->GetFavicon() : gfx::Image();
}

std::u16string Browser::GetWindowTitleForCurrentTab(
    bool include_app_name) const {
  if (!user_title_.empty())
    return base::UTF8ToUTF16(user_title_);

  // For document picture-in-picture windows, we use the title from the opener
  // WebContents instead of the picture-in-picture WebContents itself.
  content::WebContents* web_contents_for_title =
      is_type_picture_in_picture()
          ? PictureInPictureWindowManager::GetInstance()->GetWebContents()
          : tab_strip_model_->GetActiveWebContents();

  return GetWindowTitleFromWebContents(include_app_name,
                                       web_contents_for_title);
}

std::u16string Browser::GetWindowTitleForTab(int index) const {
  std::u16string title = base::UTF8ToUTF16(user_title_);

  if (title.empty()) {
    title = FormatTitleForDisplay(
        tab_strip_model_->GetWebContentsAt(index)->GetTitle());
  }

  if (title.empty() && (is_type_normal() || is_type_popup())) {
    title = CoreTabHelper::GetDefaultTitle();
  }

  return title;
}

std::u16string Browser::GetWindowTitleForMaxWidth(int max_width) const {
  static constexpr unsigned int kMinTitleCharacters = 4;
  const gfx::FontList font_list;

  if (!user_title_.empty()) {
    std::u16string title = base::UTF8ToUTF16(user_title_);
    std::u16string pixel_elided_title = gfx::ElideText(
        title, font_list, max_width, gfx::ElideBehavior::ELIDE_TAIL);
    std::u16string character_elided_title =
        gfx::TruncateString(title, kMinTitleCharacters, gfx::CHARACTER_BREAK);
    return pixel_elided_title.size() > character_elided_title.size()
               ? pixel_elided_title
               : character_elided_title;
  }

  const auto num_more_tabs = tab_strip_model_->count() - 1;
  const std::u16string format_string = l10n_util::GetPluralStringFUTF16(
      IDS_BROWSER_WINDOW_TITLE_MENU_ENTRY, num_more_tabs);

  // First, format with an empty string to see how much space we have available.
  std::u16string temp_window_title =
      base::ReplaceStringPlaceholders(format_string, std::u16string(), nullptr);
  int width = max_width - GetStringWidth(temp_window_title, font_list);

  std::u16string title;
  content::WebContents* contents = tab_strip_model_->GetActiveWebContents();
  // |contents| can be NULL if GetWindowTitleForMenu is called during the
  // window's creation (before tabs have been added).
  if (contents)
    title = FormatTitleForDisplay(app_controller_ ? app_controller_->GetTitle()
                                                  : contents->GetTitle());

  // If there is no title, leave it empty for apps.
  if (title.empty() && (is_type_normal() || is_type_popup()))
    title = CoreTabHelper::GetDefaultTitle();

  // Try to elide the title to fit the pixel width. If that will make the title
  // shorter than the minimum character limit, use a character elided title
  // instead.
  std::u16string pixel_elided_title =
      gfx::ElideText(title, font_list, width, gfx::ElideBehavior::ELIDE_TAIL);
  std::u16string character_elided_title =
      gfx::TruncateString(title, kMinTitleCharacters, gfx::CHARACTER_BREAK);
  title = pixel_elided_title.size() > character_elided_title.size()
              ? pixel_elided_title
              : character_elided_title;

  // Finally, add the page title.
  return base::ReplaceStringPlaceholders(format_string, title, nullptr);
}

std::u16string Browser::GetWindowTitleFromWebContents(
    bool include_app_name,
    content::WebContents* contents) const {
  std::u16string title = base::UTF8ToUTF16(user_title_);

  // |contents| can be NULL because GetWindowTitleForCurrentTab is called by the
  // window during the window's creation (before tabs have been added).
  if (title.empty() && contents) {
    title = FormatTitleForDisplay(app_controller_ ? app_controller_->GetTitle()
                                                  : contents->GetTitle());
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
    // If the app name is requested and this is a captive portal window, the
    // title should indicate that this is a captive portal window. Captive
    // portal windows should always be pop-ups, and the is_captive_portal_window
    // condition should not change over the lifetime of a WebContents.
    if (include_app_name &&
        captive_portal::CaptivePortalTabHelper::FromWebContents(contents) &&
        captive_portal::CaptivePortalTabHelper::FromWebContents(contents)
            ->is_captive_portal_window()) {
      DCHECK(is_type_popup());
      return l10n_util::GetStringFUTF16(
          IDS_CAPTIVE_PORTAL_BROWSER_WINDOW_TITLE_FORMAT,
          title.empty() ? CoreTabHelper::GetDefaultTitle() : title);
    }
#endif
  }

  // If there is no title, leave it empty for apps.
  if (title.empty() && (is_type_normal() || is_type_popup()))
    title = CoreTabHelper::GetDefaultTitle();

#if BUILDFLAG(IS_MAC)
  // On Mac, we don't want to suffix the page title with the application name.
  return title;
#else
  // If there is no title and this is an app, fall back on the app name. This
  // ensures that the native window gets a title which is important for a11y,
  // for example the window selector uses the Aura window title.
  if (title.empty() &&
      (is_type_app() || is_type_app_popup() || is_type_devtools()) &&
      include_app_name) {
    return app_controller_ ? app_controller_->GetAppShortName()
                           : base::UTF8ToUTF16(app_name());
  }
  // Include the app name in window titles for tabbed browser windows when
  // requested with |include_app_name|. Exception: On Lacros, when the OS is
  // collecting window titles to render for desk overview mode, this function
  // would get called with include_app_name=true. In this case,
  // include_app_name=true would be ignored and no app name would be included
  // in the title string that is to be returned. So always set
  // `include_app_name` to false.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  include_app_name = false;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  return ((is_type_normal() || is_type_popup()) && include_app_name)
             ? l10n_util::GetStringFUTF16(IDS_BROWSER_WINDOW_TITLE_FORMAT,
                                          title)
             : title;
#endif  // BUILDFLAG(IS_MAC)
}

// static
std::u16string Browser::FormatTitleForDisplay(std::u16string title) {
  size_t current_index = 0;
  size_t match_index;
  while ((match_index = title.find(L'\n', current_index)) !=
         std::u16string::npos) {
    title.replace(match_index, 1, std::u16string());
    current_index = match_index;
  }

  return title;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, OnBeforeUnload handling:

Browser::WarnBeforeClosingResult Browser::MaybeWarnBeforeClosing(
    Browser::WarnBeforeClosingCallback warn_callback) {
  // If the browser can close right away (we've indicated that we want to skip
  // before-unload handlers by setting `force_skip_warning_user_on_close_` to
  // true or there are no pending downloads we need to prompt about) then
  // there's no need to warn.
  if (force_skip_warning_user_on_close_) {
    return WarnBeforeClosingResult::kOkToClose;
  }

  // `CanCloseWithInProgressDownloads()` may trigger a modal dialog.
  bool can_close_with_downloads = CanCloseWithInProgressDownloads();
  if (can_close_with_downloads &&
      !ShouldShowCookieMigrationNoticeForBrowser(*this)) {
    return WarnBeforeClosingResult::kOkToClose;
  }

  // If there is no download warning, show the cookie migration notice now.
  // Otherwise, the download warning is being shown. Cookie migration notice
  // will be shown after, if needed.
  if (can_close_with_downloads) {
    ShowCookieClearOnExitMigrationNotice(
        *this, base::BindOnce(&Browser::CookieMigrationNoticeResponse,
                              weak_factory_.GetWeakPtr()));
  }

  DCHECK(!warn_before_closing_callback_)
      << "Tried to close window during close warning; dialog should be modal.";
  warn_before_closing_callback_ = std::move(warn_callback);

  return WarnBeforeClosingResult::kDoNotClose;
}

BrowserClosingStatus Browser::HandleBeforeClose() {
  // If `force_skip_warning_user_` is true, then we should immediately
  // return true.
  if (force_skip_warning_user_on_close_) {
    return BrowserClosingStatus::kPermitted;
  }

  // If the user needs to see one or more warnings, hold off closing the
  // browser.
  const WarnBeforeClosingResult result = MaybeWarnBeforeClosing(base::BindOnce(
      &Browser::FinishWarnBeforeClosing, weak_factory_.GetWeakPtr()));
  if (result == WarnBeforeClosingResult::kDoNotClose) {
    return BrowserClosingStatus::kDeniedByUser;
  }

  return unload_controller_.GetBrowserClosingStatus();
}

bool Browser::TryToCloseWindow(
    bool skip_beforeunload,
    const base::RepeatingCallback<void(bool)>& on_close_confirmed) {
  cancel_download_confirmation_state_ = RESPONSE_RECEIVED;
  return unload_controller_.TryToCloseWindow(skip_beforeunload,
                                             on_close_confirmed);
}

void Browser::ResetTryToCloseWindow() {
  cancel_download_confirmation_state_ = NOT_PROMPTED;
  unload_controller_.ResetTryToCloseWindow();
}

bool Browser::IsAttemptingToCloseBrowser() const {
  return unload_controller_.is_attempting_to_close_browser();
}

bool Browser::ShouldRunUnloadListenerBeforeClosing(
    content::WebContents* web_contents) {
  return !force_skip_warning_user_on_close_ &&
         unload_controller_.ShouldRunUnloadEventsHelper(web_contents);
}

bool Browser::RunUnloadListenerBeforeClosing(
    content::WebContents* web_contents) {
  return !force_skip_warning_user_on_close_ &&
         unload_controller_.RunUnloadEventsHelper(web_contents);
}

void Browser::SetWindowUserTitle(const std::string& user_title) {
  user_title_ = user_title;
  window_->UpdateTitleBar();
  // See comment in Browser::OnTabGroupChanged
  DCHECK(!IsRelevantToAppSessionService(type_));
  SessionService* const session_service =
      SessionServiceFactory::GetForProfile(profile_);
  if (session_service)
    session_service->SetWindowUserTitle(session_id(), user_title);
}

Browser* Browser::GetBrowserForOpeningWebUi() {
  if (!is_type_picture_in_picture()) {
    return this;
  }

  if (!opener_browser_) {
    auto* opener_web_contents =
        PictureInPictureWindowManager::GetInstance()->GetWebContents();
    // We should always have an opener web contents if the current browser is a
    // picture-in-picture type.
    DCHECK(opener_web_contents);
    opener_browser_ = chrome::FindBrowserWithTab(opener_web_contents);
  }

  return opener_browser_;
}

StatusBubble* Browser::GetStatusBubbleForTesting() {
  return GetStatusBubble();
}

void Browser::SetForceShowBookmarkBarFlag(ForceShowBookmarkBarFlag flag) {
  force_show_bookmark_bar_flags_ |= flag;
  UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_FORCE_SHOW);
}

void Browser::ClearForceShowBookmarkBarFlag(ForceShowBookmarkBarFlag flag) {
  force_show_bookmark_bar_flags_ &= ~flag;
  UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_FORCE_SHOW);
}

views::WebView* Browser::GetWebView() {
  return window_->GetContentsWebView();
}

Profile* Browser::GetProfile() {
  return profile();
}

void Browser::OpenGURL(const GURL& gurl, WindowOpenDisposition disposition) {
  OpenURL(content::OpenURLParams(gurl, content::Referrer(), disposition,
                                 ui::PAGE_TRANSITION_LINK,
                                 /*is_renderer_initiated=*/false),
          /*navigation_handle_callback=*/{});
}

const SessionID& Browser::GetSessionID() {
  return session_id_;
}

TabStripModel* Browser::GetTabStripModel() {
  return tab_strip_model_.get();
}

bool Browser::IsTabStripVisible() {
  return window_ && window_->IsToolbarShowing();
}

bool Browser::ShouldHideUIForFullscreen() const {
  // Windows and GTK remove the browser controls in fullscreen, but Mac and Ash
  // keep the controls in a slide-down panel.
  return window_ && window_->ShouldHideUIForFullscreen();
}

views::View* Browser::TopContainer() {
  return window_->GetTopContainer();
}

base::CallbackListSubscription Browser::RegisterActiveTabDidChange(
    ActiveTabChangeCallback callback) {
  return did_active_tab_change_callback_list_.Add(std::move(callback));
}

tabs::TabInterface* Browser::GetActiveTabInterface() {
  return tab_strip_model_->GetActiveTab();
}

BrowserWindowFeatures& Browser::GetFeatures() {
  return *features_.get();
}

web_modal::WebContentsModalDialogHost*
Browser::GetWebContentsModalDialogHostForWindow() {
  return window_->GetWebContentsModalDialogHost();
}

bool Browser::IsActive() {
  return window_->IsActive();
}

base::CallbackListSubscription Browser::RegisterDidBecomeActive(
    DidBecomeActiveCallback callback) {
  return did_become_active_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription Browser::RegisterDidBecomeInactive(
    DidBecomeInactiveCallback callback) {
  return did_become_inactive_callback_list_.Add(std::move(callback));
}

ExclusiveAccessManager* Browser::GetExclusiveAccessManager() {
  return exclusive_access_manager();
}

BrowserActions* Browser::GetActions() {
  return browser_actions();
}

BrowserWindowInterface::Type Browser::GetType() const {
  return type_;
}

BrowserUserEducationInterface* Browser::GetUserEducationInterface() {
  return window();
}

web_app::AppBrowserController* Browser::GetAppBrowserController() {
  return app_controller_.get();
}

void Browser::DidBecomeActive() {
  BrowserList::SetLastActive(this);
  did_become_active_callback_list_.Notify(this);
}

void Browser::DidBecomeInactive() {
  BrowserList::NotifyBrowserNoLongerActive(this);
  did_become_inactive_callback_list_.Notify(this);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool Browser::IsLockedForOnTask() {
  return on_task_locked_;
}

void Browser::SetLockedForOnTask(bool locked) {
  on_task_locked_ = locked;
  OnLockedForOnTaskUpdated();
}
#endif

void Browser::OnWindowClosing() {
  if (const auto closing_status = HandleBeforeClose();
      closing_status != BrowserClosingStatus::kPermitted) {
    BrowserList::NotifyBrowserCloseCancelled(this, closing_status);
    return;
  }

  // Application should shutdown on last window close if the user is explicitly
  // trying to quit, or if there is nothing keeping the browser alive (such as
  // AppController on the Mac, or BackgroundContentsService for background
  // pages).
  bool should_quit_if_last_browser =
      browser_shutdown::IsTryingToQuit() ||
      KeepAliveRegistry::GetInstance()->IsKeepingAliveOnlyByBrowserOrigin();

  if (should_quit_if_last_browser && ShouldStartShutdown()) {
    browser_shutdown::OnShutdownStarting(
        browser_shutdown::ShutdownType::kWindowClose);
  }

  // Don't use GetForProfileIfExisting here, we want to force creation of the
  // session service so that user can restore what was open.
  SessionServiceBase* service = GetAppropriateSessionServiceForProfile(this);

  if (service)
    service->WindowClosing(session_id());

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile());

  bool notify_restore_service = is_type_normal() && tab_strip_model_->count();
#if defined(USE_AURA) || BUILDFLAG(IS_MAC)
  notify_restore_service |= is_type_app() || is_type_app_popup();
#endif

  if (tab_restore_service && notify_restore_service)
    tab_restore_service->BrowserClosing(live_tab_context());

  BrowserList::NotifyBrowserCloseStarted(this);

  if (!tab_strip_model_->empty()) {
    // Closing all the tabs results in eventually calling back to
    // OnWindowClosing() again.
    tab_strip_model_->CloseAllTabs();
  } else {
    // If there are no tabs, then a task will be scheduled (by views) to delete
    // this Browser.
    is_delete_scheduled_ = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
// In-progress download termination handling:

Browser::DownloadCloseType Browser::OkToCloseWithInProgressDownloads(
    int* num_downloads_blocking) const {
  DCHECK(num_downloads_blocking);
  *num_downloads_blocking = 0;

  // If we're not running a full browser process with a profile manager
  // (testing), it's ok to close the browser.
  if (!g_browser_process->profile_manager())
    return DownloadCloseType::kOk;

  int total_download_count =
      DownloadCoreService::BlockingShutdownCountAllProfiles();
  if (total_download_count == 0)
    return DownloadCloseType::kOk;  // No downloads; can definitely close.

  // Figure out how many windows are open total, and associated with this
  // profile, that are relevant for the ok-to-close decision.
  auto [last_window, last_window_for_profile] = IsLastWindow(*this);

  // If there aren't any other windows, we're at browser shutdown,
  // which would cancel all current downloads.
  if (last_window) {
    *num_downloads_blocking = total_download_count;
    return DownloadCloseType::kBrowserShutdown;
  }

  // If there aren't any other windows on our profile, and we're an Incognito
  // or Guest profile, and there are downloads associated with that profile,
  // those downloads would be cancelled by our window (-> profile) close.
  DownloadCoreService* download_core_service =
      DownloadCoreServiceFactory::GetForBrowserContext(profile());
  if (last_window_for_profile &&
      (download_core_service->BlockingShutdownCount() > 0) &&
      (profile()->IsIncognitoProfile() || profile()->IsGuestSession())) {
    *num_downloads_blocking = download_core_service->BlockingShutdownCount();
    return profile()->IsGuestSession()
               ? DownloadCloseType::kLastWindowInGuestSession
               : DownloadCloseType::kLastWindowInIncognitoProfile;
  }

  // Those are the only conditions under which we will block shutdown.
  return DownloadCloseType::kOk;
}

////////////////////////////////////////////////////////////////////////////////
// Browser, Tab adding/showing functions:

void Browser::WindowFullscreenStateChanged() {
  exclusive_access_manager_->fullscreen_controller()
      ->WindowFullscreenStateChanged();
  command_controller_->FullscreenStateChanged();
  UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TOGGLE_FULLSCREEN);
}

void Browser::FullscreenTopUIStateChanged() {
  command_controller_->FullscreenStateChanged();
  UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TOOLBAR_OPTION_CHANGE);
}

void Browser::OnFindBarVisibilityChanged() {
  window()->UpdatePageActionIcon(PageActionIconType::kFind);
  command_controller_->FindBarVisibilityChanged();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Assorted browser commands:

void Browser::ToggleFullscreenModeWithExtension(const GURL& extension_url) {
  exclusive_access_manager_->fullscreen_controller()
      ->ToggleBrowserFullscreenModeWithExtension(extension_url);
}

bool Browser::SupportsWindowFeature(WindowFeature feature) const {
  bool supports =
      SupportsWindowFeatureImpl(feature, /*check_can_support=*/false);
  // Supported features imply CanSupportWindowFeature.
  DCHECK(!supports || CanSupportWindowFeature(feature));
  return supports;
}

bool Browser::CanSupportWindowFeature(WindowFeature feature) const {
  return SupportsWindowFeatureImpl(feature, /*check_can_support=*/true);
}

void Browser::OpenFile() {
  // Ignore if there is already a select file dialog.
  if (select_file_dialog_)
    return;

  base::RecordAction(UserMetricsAction("OpenFile"));
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(
                tab_strip_model_->GetActiveWebContents()));

  if (!select_file_dialog_)
    return;

  const base::FilePath directory = profile_->last_selected_directory();
  // TODO(beng): figure out how to juggle this.
  gfx::NativeWindow parent_window = window_->GetNativeWindow();
  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.allowed_paths =
      ui::SelectFileDialog::FileTypeInfo::ANY_PATH_OR_URL;
  select_file_dialog_->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                  std::u16string(), directory, &file_types, 0,
                                  base::FilePath::StringType(), parent_window);
}

void Browser::UpdateDownloadShelfVisibility(bool visible) {
  if (GetStatusBubble())
    GetStatusBubble()->UpdateDownloadShelfVisibility(visible);
}

bool Browser::CanSaveContents(content::WebContents* web_contents) const {
  return chrome::CanSavePage(this);
}

bool Browser::ShouldDisplayFavicon(content::WebContents* web_contents) const {
  // Remove for all other tabbed web apps.
  if (app_controller_ && app_controller_->has_tab_strip())
    return false;

  // Otherwise, always display the favicon.
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void Browser::UpdateUIForNavigationInTab(WebContents* contents,
                                         ui::PageTransition transition,
                                         NavigateParams::WindowAction action,
                                         bool user_initiated) {
  tab_strip_model_->TabNavigating(contents, transition);

  bool contents_is_selected =
      contents == tab_strip_model_->GetActiveWebContents();
  if (user_initiated && contents_is_selected && window()->GetLocationBar()) {
    // Forcibly reset the location bar if the url is going to change in the
    // current tab, since otherwise it won't discard any ongoing user edits,
    // since it doesn't realize this is a user-initiated action.
    window()->GetLocationBar()->Revert();
  }

  if (GetStatusBubble())
    GetStatusBubble()->Hide();

  // Update the location bar. This is synchronous. We specifically don't
  // update the load state since the load hasn't started yet and updating it
  // will put it out of sync with the actual state like whether we're
  // displaying a favicon, which controls the throbber. If we updated it here,
  // the throbber will show the default favicon for a split second when
  // navigating away from the new tab page.
  ScheduleUIUpdate(contents, content::INVALIDATE_TYPE_URL);

  // Navigating contents can take focus (potentially taking it away from other,
  // currently-focused UI element like the omnibox) if the navigation was
  // initiated by the user (e.g., via omnibox, bookmarks, etc.).
  //
  // Note that focusing contents of NTP-initiated navigations is taken care of
  // elsewhere - see FocusTabAfterNavigationHelper.
  if (user_initiated && contents_is_selected &&
      (window()->IsActive() || action == NavigateParams::SHOW_WINDOW)) {
    contents->SetInitialFocus();
  }
}

void Browser::RegisterKeepAlive() {
  keep_alive_ = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::BROWSER, KeepAliveRestartOption::DISABLED);
}
void Browser::UnregisterKeepAlive() {
  keep_alive_.reset();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, PageNavigator implementation:

WebContents* Browser::OpenURL(
    const OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
#if DCHECK_IS_ON()
  DCHECK(params.Valid());
#endif

  return OpenURLFromTab(nullptr, params, std::move(navigation_handle_callback));
}

///////////////////////////////////////////////////////////////////////////////
// Browser, TabStripModelObserver implementation:

void Browser::OnTabStripModelChanged(TabStripModel* tab_strip_model,
                                     const TabStripModelChange& change,
                                     const TabStripSelectionChange& selection) {
  TRACE_EVENT2("ui", "Browser::OnTabStripModelChanged", "tab_strip_model",
               tab_strip_model, "change", change);
  switch (change.type()) {
    case TabStripModelChange::kInserted: {
      for (const auto& contents : change.GetInsert()->contents)
        OnTabInsertedAt(contents.contents, contents.index);
      break;
    }
    case TabStripModelChange::kRemoved: {
      for (const auto& contents : change.GetRemove()->contents) {
        if (contents.remove_reason ==
            TabStripModelChange::RemoveReason::kDeleted)
          OnTabClosing(contents.contents);
        OnTabDetached(contents.contents,
                      contents.contents == selection.old_contents);
      }
      break;
    }
    case TabStripModelChange::kMoved: {
      auto* move = change.GetMove();
      OnTabMoved(move->from_index, move->to_index);
      break;
    }
    case TabStripModelChange::kReplaced: {
      auto* replace = change.GetReplace();
      OnTabReplacedAt(replace->old_contents, replace->new_contents,
                      replace->index);
      break;
    }
    case TabStripModelChange::kSelectionOnly:
      break;
  }

  if (!selection.active_tab_changed())
    return;

  if (selection.old_contents)
    OnTabDeactivated(selection.old_contents);

  if (tab_strip_model_->empty())
    return;

  OnActiveTabChanged(
      selection.old_contents, selection.new_contents,
      selection.new_model.active().has_value()
          ? static_cast<int>(selection.new_model.active().value())
          : TabStripModel::kNoTab,
      selection.reason);
}

void Browser::OnTabGroupChanged(const TabGroupChange& change) {
  // If apps ever get tab grouping, this function needs to be updated to
  // retrieve AppSessionService from the correct factory. Additionally,
  // AppSessionService doesn't support SetTabGroupMetadata, so some
  // work to refactor the code to support that into SessionServiceBase
  // would be the best way to achieve that.
  DCHECK(!IsRelevantToAppSessionService(type_));
  DCHECK(tab_strip_model_->group_model());
  if (change.type == TabGroupChange::kVisualsChanged) {
    std::optional<std::string> saved_guid;

    tab_groups::TabGroupSyncService* tab_group_service =
        tab_groups::SavedTabGroupUtils::GetServiceForProfile(profile_);
    if (tab_group_service) {
      const std::optional<tab_groups::SavedTabGroup> saved_group =
          tab_group_service->GetGroup(change.group);
      if (saved_group) {
        saved_guid = saved_group->saved_guid().AsLowercaseString();
      }
    }
    UpdateTabGroupSessionMetadata(this, change.group, std::move(saved_guid));
  } else if (change.type == TabGroupChange::kClosed) {
    sessions::TabRestoreService* tab_restore_service =
        TabRestoreServiceFactory::GetForProfile(profile());
    if (tab_restore_service)
      tab_restore_service->GroupClosed(change.group);
  }
}

void Browser::TabPinnedStateChanged(TabStripModel* tab_strip_model,
                                    WebContents* contents,
                                    int index) {
  // See comment in Browser::OnTabGroupChanged
  DCHECK(!IsRelevantToAppSessionService(type_));
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(profile());
  if (session_service) {
    sessions::SessionTabHelper* session_tab_helper =
        sessions::SessionTabHelper::FromWebContents(contents);
    session_service->SetPinnedState(session_id(),
                                    session_tab_helper->session_id(),
                                    tab_strip_model_->IsTabPinned(index));
  }
}

void Browser::TabGroupedStateChanged(
    std::optional<tab_groups::TabGroupId> group,
    tabs::TabModel* tab,
    int index) {
  // See comment in Browser::OnTabGroupChanged
  DCHECK(!IsRelevantToAppSessionService(type_));
  SessionService* const session_service =
      SessionServiceFactory::GetForProfile(profile_);
  if (!session_service)
    return;

  sessions::SessionTabHelper* const session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(tab->contents());
  session_service->SetTabGroup(session_id(), session_tab_helper->session_id(),
                               std::move(group));
}

void Browser::TabStripEmpty() {
  // Note: even though the tab strip is empty, the call to Close() may not
  // result in closing this Browser. This can happen in the case of closing
  // the last Browser with ongoing downloads.
  window_->Close();

  // Instant may have visible WebContents that need to be detached before the
  // window system closes.
  instant_controller_.reset();
}

void Browser::SetTopControlsShownRatio(content::WebContents* web_contents,
                                       float ratio) {
  window_->SetTopControlsShownRatio(web_contents, ratio);
}

int Browser::GetTopControlsHeight() {
  return window_->GetTopControlsHeight();
}

bool Browser::DoBrowserControlsShrinkRendererSize(
    content::WebContents* contents) {
  return window_->DoBrowserControlsShrinkRendererSize(contents);
}

int Browser::GetVirtualKeyboardHeight(content::WebContents* contents) {
  // This API is currently only used by View Transitions when the virtual
  // keyboard resizes content.  On desktop platforms, the virtual keyboard can
  // only inset the visual viewport so it shouldn't ever be called.
  NOTIMPLEMENTED();
  return 0;
}

void Browser::SetTopControlsGestureScrollInProgress(bool in_progress) {
  window_->SetTopControlsGestureScrollInProgress(in_progress);
}

bool Browser::CanOverscrollContent() {
#if defined(USE_AURA)
  return !is_type_devtools() &&
         base::FeatureList::IsEnabled(features::kOverscrollHistoryNavigation) &&
         overscroll_pref_manager_->IsOverscrollHistoryNavigationEnabled();
#else
  return false;
#endif
}

bool Browser::ShouldPreserveAbortedURLs(WebContents* source) {
  // Allow failed URLs to stick around in the omnibox on the NTP, but not when
  // other pages have committed.
  Profile* profile = Profile::FromBrowserContext(source->GetBrowserContext());
  if (!profile || !source->GetController().GetLastCommittedEntry())
    return false;
  GURL committed_url(source->GetController().GetLastCommittedEntry()->GetURL());
  return search::IsNTPOrRelatedURL(committed_url, profile);
}

void Browser::SetFocusToLocationBar() {
  // Two differences between this and FocusLocationBar():
  // (1) This doesn't get recorded in user metrics, since it's called
  //     internally.
  // (2) This is called with |is_user_initiated| == false, because this is a
  //     renderer initiated focus (this method is a WebContentsDelegate
  //     override).
  window_->SetFocusToLocationBar(false);
}

content::KeyboardEventProcessingResult Browser::PreHandleKeyboardEvent(
    content::WebContents* source,
    const NativeWebKeyboardEvent& event) {
  // Forward keyboard events to the manager for fullscreen / mouse lock. This
  // may consume the event (e.g., Esc exits fullscreen mode).
  // TODO(koz): Write a test for this http://crbug.com/100441.
  if (exclusive_access_manager_->HandleUserKeyEvent(event))
    return content::KeyboardEventProcessingResult::HANDLED;

  return window()->PreHandleKeyboardEvent(event);
}

bool Browser::HandleKeyboardEvent(content::WebContents* source,
                                  const NativeWebKeyboardEvent& event) {
  DevToolsWindow* devtools_window =
      DevToolsWindow::GetInstanceForInspectedWebContents(source);
  return (devtools_window && devtools_window->ForwardKeyboardEvent(event)) ||
         window()->HandleKeyboardEvent(event);
}

bool Browser::TabsNeedBeforeUnloadFired() const {
  return unload_controller_.TabsNeedBeforeUnloadFired();
}

bool Browser::PreHandleGestureEvent(content::WebContents* source,
                                    const blink::WebGestureEvent& event) {
  // Disable pinch zooming in undocked dev tools window due to poor UX.
  if (app_name() == DevToolsWindow::kDevToolsApp)
    return blink::WebInputEvent::IsPinchGestureEventType(event.GetType());
  return false;
}

bool Browser::CanDragEnter(content::WebContents* source,
                           const content::DropData& data,
                           blink::DragOperationsMask operations_allowed) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Disallow drag-and-drop navigation for Settings windows which do not support
  // external navigation.
  if ((operations_allowed & blink::kDragOperationLink) &&
      chrome::SettingsWindowManager::GetInstance()->IsSettingsBrowser(this)) {
    return false;
  }
#endif
  return true;
}

void Browser::CreateSmsPrompt(content::RenderFrameHost*,
                              const std::vector<url::Origin>&,
                              const std::string& one_time_code,
                              base::OnceClosure on_confirm,
                              base::OnceClosure on_cancel) {
  // TODO(crbug.com/40103792): implementation left pending deliberately.
  std::move(on_confirm).Run();
}

bool Browser::ShouldAllowRunningInsecureContent(
    content::WebContents* web_contents,
    bool allowed_per_prefs,
    const url::Origin& origin,
    const GURL& resource_url) {
  // Note: this implementation is a mirror of
  // ContentSettingsObserver::allowRunningInsecureContent.
  if (allowed_per_prefs)
    return true;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile);
  return content_settings->GetContentSetting(
             web_contents->GetLastCommittedURL(), GURL(),
             ContentSettingsType::MIXEDSCRIPT) == CONTENT_SETTING_ALLOW;
}

void Browser::OnDidBlockNavigation(
    content::WebContents* web_contents,
    const GURL& blocked_url,
    const GURL& initiator_url,
    blink::mojom::NavigationBlockedReason reason) {
  if (reason ==
      blink::mojom::NavigationBlockedReason::kRedirectWithNoUserGesture) {
    if (auto* framebust_helper =
            FramebustBlockTabHelper::FromWebContents(web_contents)) {
      auto on_click = [](const GURL& url, size_t index, size_t total_elements) {
        UMA_HISTOGRAM_ENUMERATION(
            "WebCore.Framebust.ClickThroughPosition",
            blocked_content::GetListItemPositionFromDistance(index,
                                                             total_elements));
      };
      framebust_helper->AddBlockedUrl(blocked_url, base::BindOnce(on_click));
    }
  }
}

content::PictureInPictureResult Browser::EnterPictureInPicture(
    content::WebContents* web_contents) {
  return PictureInPictureWindowManager::GetInstance()
      ->EnterVideoPictureInPicture(web_contents);
}

void Browser::ExitPictureInPicture() {
  PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
}

bool Browser::IsBackForwardCacheSupported(content::WebContents& web_contents) {
  return true;
}

content::PreloadingEligibility Browser::IsPrerender2Supported(
    content::WebContents& web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents.GetBrowserContext());
  return prefetch::IsSomePreloadingEnabled(*profile->GetPrefs());
}

bool Browser::ShouldShowStaleContentOnEviction(content::WebContents* source) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return source == tab_strip_model_->GetActiveWebContents();
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// TODO(crbug.com/40177301): Remove this.
void Browser::MediaWatchTimeChanged(
    const content::MediaPlayerWatchTime& watch_time) {}

bool Browser::IsPointerLocked() const {
  return exclusive_access_manager_->pointer_lock_controller()
      ->IsPointerLocked();
}

void Browser::OnWindowDidShow() {
  if (window_has_shown_)
    return;
  window_has_shown_ = true;

  startup_metric_utils::GetBrowser().RecordBrowserWindowDisplay(
      base::TimeTicks::Now());

  // Nothing to do for non-tabbed windows.
  if (!is_type_normal())
    return;

  // Show any pending global error bubble.
  GlobalErrorService* service =
      GlobalErrorServiceFactory::GetForProfile(profile());
  GlobalError* error = service->GetFirstGlobalErrorWithBubbleView();
  if (error)
    error->ShowBubbleView(this);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, content::WebContentsDelegate implementation:

WebContents* Browser::OpenURLFromTab(
    WebContents* source,
    const OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  TRACE_EVENT1("navigation", "Browser::OpenURLFromTab", "source", source);
#if DCHECK_IS_ON()
  DCHECK(params.Valid());
#endif

  if (is_type_devtools()) {
    DevToolsWindow* window = DevToolsWindow::AsDevToolsWindow(source);
    DCHECK(window);
    return window->OpenURLFromTab(source, params,
                                  std::move(navigation_handle_callback));
  }

  NavigateParams nav_params(this, params.url, params.transition);
  nav_params.FillNavigateParamsFromOpenURLParams(params);
  nav_params.source_contents = source;
  nav_params.tabstrip_add_types = AddTabTypes::ADD_NONE;
  if (params.user_gesture)
    nav_params.window_action = NavigateParams::SHOW_WINDOW;
  bool is_popup =
      source && blocked_content::ConsiderForPopupBlocking(params.disposition);
  auto popup_delegate =
      std::make_unique<ChromePopupNavigationDelegate>(std::move(nav_params));
  if (is_popup) {
    popup_delegate.reset(static_cast<ChromePopupNavigationDelegate*>(
        blocked_content::MaybeBlockPopup(
            source, nullptr, std::move(popup_delegate), &params,
            blink::mojom::WindowFeatures(),
            HostContentSettingsMapFactory::GetForProfile(
                source->GetBrowserContext()))
            .release()));
    if (!popup_delegate)
      return nullptr;
  }

  chrome::ConfigureTabGroupForNavigation(popup_delegate->nav_params());

  base::WeakPtr<content::NavigationHandle> navigation_handle =
      Navigate(popup_delegate->nav_params());

  if (navigation_handle_callback && navigation_handle) {
    std::move(navigation_handle_callback).Run(*navigation_handle);
  }

  content::WebContents* navigated_or_inserted_contents =
      popup_delegate->nav_params()->navigated_or_inserted_contents;
  if (is_popup && navigated_or_inserted_contents) {
    auto* tracker = blocked_content::PopupTracker::CreateForWebContents(
        navigated_or_inserted_contents, source, params.disposition);
    tracker->set_is_trusted(
        params.triggering_event_info !=
        blink::mojom::TriggeringEventInfo::kFromUntrustedEvent);
  }

  TRACE_EVENT_INSTANT1(
      "navigation", "Browser::OpenURLFromTab_Result", TRACE_EVENT_SCOPE_THREAD,
      "navigated_or_inserted_contents", navigated_or_inserted_contents);

  return navigated_or_inserted_contents;
}

void Browser::NavigationStateChanged(WebContents* source,
                                     content::InvalidateTypes changed_flags) {
  // If we're shutting down we should refuse to process this message.
  // See crbug.com/1306297; it's possible that a WebContents sends navigation
  // state messages while destructing during browser tear-down. Ironically we
  // can't use IsShuttingDown() because by this point the browser is entirely
  // removed from the browser list.
  if (!command_controller_)
    return;

  // Only update the UI when something visible has changed.
  if (changed_flags)
    ScheduleUIUpdate(source, changed_flags);

  // We can synchronously update commands since they will only change once per
  // navigation, so we don't have to worry about flickering. We do, however,
  // need to update the command state early on load to always present usable
  // actions in the face of slow-to-commit pages.
  if (changed_flags &
      (content::INVALIDATE_TYPE_URL | content::INVALIDATE_TYPE_LOAD |
       content::INVALIDATE_TYPE_TAB))
    command_controller_->TabStateChanged();

  if (app_controller_)
    app_controller_->UpdateCustomTabBarVisibility(true);
}

void Browser::VisibleSecurityStateChanged(WebContents* source) {
  // When the current tab's security state changes, we need to update the URL
  // bar to reflect the new state.
  DCHECK(source);
  if (tab_strip_model_->GetActiveWebContents() == source) {
    UpdateToolbarSecurityState();

    if (app_controller_) {
      app_controller_->UpdateCustomTabBarVisibility(true);
    }
  }
}

content::WebContents* Browser::AddNewContents(
    WebContents* source,
    std::unique_ptr<WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  FullscreenController* fullscreen_controller =
      exclusive_access_manager_->fullscreen_controller();
#if BUILDFLAG(IS_MAC)
  // On the Mac, the convention is to turn popups into new tabs when in browser
  // fullscreen mode. Only worry about user-initiated fullscreen as showing a
  // popup in HTML5 fullscreen would have kicked the page out of fullscreen.
  // However if this Browser is for an app or the popup is being requested on a
  // different display, we don't want to turn popups into new tabs. Popups
  // should open as new windows instead.
  display::Screen* screen = display::Screen::GetScreen();
  bool targeting_different_display =
      screen && source && source->GetContentNativeView() &&
      screen->GetDisplayNearestView(source->GetContentNativeView()) !=
          screen->GetDisplayMatching(window_features.bounds);
  if (!app_controller_ && disposition == WindowOpenDisposition::NEW_POPUP &&
      fullscreen_controller->IsFullscreenForBrowser() &&
      !targeting_different_display) {
    disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  }
#endif

  // At this point the |new_contents| is beyond the popup blocker, but we use
  // the same logic for determining if the popup tracker needs to be attached.
  if (source && blocked_content::ConsiderForPopupBlocking(disposition)) {
    blocked_content::PopupTracker::CreateForWebContents(new_contents.get(),
                                                        source, disposition);
  }

  // Postpone activating popups opened by content-fullscreen tabs. This permits
  // popups on other screens and retains fullscreen focus for exit accelerators.
  // Popups are activated when the opener exits fullscreen, which happens
  // immediately if the popup would overlap the fullscreen window.
  // Allow fullscreen-within-tab openers to open popups normally.
  NavigateParams::WindowAction window_action = NavigateParams::SHOW_WINDOW;
  if (disposition == WindowOpenDisposition::NEW_POPUP &&
      GetFullscreenState(source).target_mode ==
          content::FullscreenMode::kContent) {
    window_action = NavigateParams::SHOW_WINDOW_INACTIVE;
    fullscreen_controller->FullscreenTabOpeningPopup(source,
                                                     new_contents.get());
    // Defer popup creation if the opener has a fullscreen transition in
    // progress. This works around a defect on Mac where separate displays
    // cannot switch their independent spaces simultaneously (crbug.com/1315749)
    auto web_contents_creation_callback = base::BindOnce(
        &chrome::AddWebContents, this, source, std::move(new_contents),
        target_url, disposition, window_features, window_action);
    fullscreen_controller->RunOrDeferUntilTransitionIsComplete(base::BindOnce(
        base::IgnoreResult(std::move(web_contents_creation_callback))));
    return nullptr;
  }

  return chrome::AddWebContents(this, source, std::move(new_contents),
                                target_url, disposition, window_features,
                                window_action);
}

void Browser::ActivateContents(WebContents* contents) {
  // A WebContents can ask to activate after it's been removed from the
  // TabStripModel. See https://crbug.com/1060986
  int index = tab_strip_model_->GetIndexOfWebContents(contents);
  if (index == TabStripModel::kNoTab)
    return;
  tab_strip_model_->ActivateTabAt(index);
  window_->Activate();
}

void Browser::LoadingStateChanged(WebContents* source,
                                  bool should_show_loading_ui) {
  ScheduleUIUpdate(source, content::INVALIDATE_TYPE_LOAD);
  UpdateWindowForLoadingStateChanged(source, should_show_loading_ui);
}

void Browser::CloseContents(WebContents* source) {
  if (unload_controller_.CanCloseContents(source))
    chrome::CloseWebContents(this, source, true);
}

void Browser::SetContentsBounds(WebContents* source, const gfx::Rect& bounds) {
  if (is_type_normal()) {
    return;
  }

  std::vector<blink::mojom::WebFeature> features = {
      blink::mojom::WebFeature::kMovedOrResizedPopup};
  if (creation_timer_.Elapsed() > base::Seconds(2)) {
    // Additionally measure whether a popup was moved after creation, to
    // distinguish between popups that reposition themselves after load and
    // those which move popups continuously.
    features.push_back(
        blink::mojom::WebFeature::kMovedOrResizedPopup2sAfterCreation);
  }

  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      source->GetPrimaryMainFrame(), std::move(features));
  window_->SetBounds(bounds);
}

void Browser::UpdateTargetURL(WebContents* source, const GURL& url) {
  if (!GetStatusBubble())
    return;

  if (source == tab_strip_model_->GetActiveWebContents())
    GetStatusBubble()->SetURL(url);
}

void Browser::ContentsMouseEvent(WebContents* source, const ui::Event& event) {
  const ui::EventType type = event.type();
  const bool exited = type == ui::EventType::kMouseExited;
  // Disregard synthesized events, and mouse enter and exit, which may occur
  // without explicit user input events during window state changes.
  if (type != ui::EventType::kMouseEntered && !exited &&
      !event.IsSynthesized()) {
    exclusive_access_manager_->OnUserInput();
  }

  // Mouse motion events update the status bubble, if it exists.
  if (GetStatusBubble() && source == tab_strip_model_->GetActiveWebContents() &&
      (type == ui::EventType::kMouseMoved || exited)) {
    GetStatusBubble()->MouseMoved(exited);
    if (exited) {
      GetStatusBubble()->SetURL(GURL());
    }
  }
}

void Browser::ContentsZoomChange(bool zoom_in) {
  chrome::ExecuteCommand(this, zoom_in ? IDC_ZOOM_PLUS : IDC_ZOOM_MINUS);
}

bool Browser::TakeFocus(content::WebContents* source, bool reverse) {
  return false;
}

void Browser::BeforeUnloadFired(WebContents* web_contents,
                                bool proceed,
                                bool* proceed_to_fire_unload) {
  if (is_type_devtools() && DevToolsWindow::HandleBeforeUnload(
                                web_contents, proceed, proceed_to_fire_unload))
    return;

  *proceed_to_fire_unload =
      unload_controller_.BeforeUnloadFired(web_contents, proceed);
}

bool Browser::ShouldFocusLocationBarByDefault(WebContents* source) {
  // Navigations in background tabs shouldn't change the focus state of the
  // omnibox, since it's associated with the foreground tab.
  if (source != tab_strip_model_->GetActiveWebContents())
    return false;

  // This should be based on the pending entry if there is one, so that
  // back/forward navigations to the NTP are handled.  The visible entry can't
  // be used here, since back/forward navigations are not treated as visible
  // entries to avoid URL spoofs.
  content::NavigationEntry* entry =
      source->GetController().GetPendingEntry()
          ? source->GetController().GetPendingEntry()
          : source->GetController().GetLastCommittedEntry();
  if (entry) {
    const GURL& url = entry->GetURL();
    const GURL& virtual_url = entry->GetVirtualURL();

    if (virtual_url.SchemeIs(content::kViewSourceScheme))
      return false;

    if ((url.SchemeIs(content::kChromeUIScheme) &&
         url.host_piece() == chrome::kChromeUINewTabHost) ||
        (virtual_url.SchemeIs(content::kChromeUIScheme) &&
         virtual_url.host_piece() == chrome::kChromeUINewTabHost)) {
      return true;
    }
  }

  return search::NavEntryIsInstantNTP(source, entry);
}

bool Browser::ShouldFocusPageAfterCrash(WebContents* source) {
  // Focus only the active page when reloading after a crash, otherwise
  // return false. This is to ensure background reloads via hovercard
  // don't end up causing a focus loss which results in its dismissal.
  return source == tab_strip_model_->GetActiveWebContents();
}

void Browser::ShowRepostFormWarningDialog(WebContents* source) {
  TabModalConfirmDialog::Create(
      std::make_unique<RepostFormWarningController>(source), source);
}

bool Browser::IsWebContentsCreationOverridden(
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  return window_container_type ==
             content::mojom::WindowContainerType::BACKGROUND &&
         ShouldCreateBackgroundContents(source_site_instance, opener_url,
                                        frame_name);
}

WebContents* Browser::CreateCustomWebContents(
    content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance,
    bool is_new_browsing_instance,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url,
    const content::StoragePartitionConfig& partition_config,
    content::SessionStorageNamespace* session_storage_namespace) {
  BackgroundContents* background_contents = CreateBackgroundContents(
      source_site_instance, opener, opener_url, is_new_browsing_instance,
      frame_name, target_url, partition_config, session_storage_namespace);
  if (background_contents) {
    return background_contents->web_contents();
  }
  return nullptr;
}

void Browser::WebContentsCreated(WebContents* source_contents,
                                 int opener_render_process_id,
                                 int opener_render_frame_id,
                                 const std::string& frame_name,
                                 const GURL& target_url,
                                 WebContents* new_contents) {
  // Adopt the WebContents now, so all observers are in place, as the network
  // requests for its initial navigation will start immediately. The WebContents
  // will later be inserted into this browser using Browser::Navigate via
  // AddNewContents.
  TabHelpers::AttachTabHelpers(new_contents);

  // Make the tab show up in the task manager.
  task_manager::WebContentsTags::CreateForTabContents(new_contents);
}

void Browser::RendererUnresponsive(
    WebContents* source,
    content::RenderWidgetHost* render_widget_host,
    base::RepeatingClosure hang_monitor_restarter) {
  // Don't show the page hung dialog when a HTML popup hangs because
  // the dialog will take the focus and immediately close the popup.
  RenderWidgetHostView* view = render_widget_host->GetView();
  if (view && !render_widget_host->GetView()->IsHTMLFormPopup()) {
    TabDialogs::FromWebContents(source)->ShowHungRendererDialog(
        render_widget_host, std::move(hang_monitor_restarter));
  }
}

void Browser::RendererResponsive(
    WebContents* source,
    content::RenderWidgetHost* render_widget_host) {
  RenderWidgetHostView* view = render_widget_host->GetView();
  if (view && !render_widget_host->GetView()->IsHTMLFormPopup()) {
    TabDialogs::FromWebContents(source)->HideHungRendererDialog(
        render_widget_host);
  }
}

content::JavaScriptDialogManager* Browser::GetJavaScriptDialogManager(
    WebContents* source) {
  return javascript_dialogs::TabModalDialogManager::FromWebContents(source);
}

bool Browser::GuestSaveFrame(content::WebContents* guest_web_contents) {
  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromWebContents(guest_web_contents);
  return guest_view && guest_view->PluginDoSave();
}

std::unique_ptr<content::EyeDropper> Browser::OpenEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  return window()->OpenEyeDropper(frame, listener);
}

void Browser::InitiatePreview(content::WebContents& web_contents,
                              const GURL& url) {
#if !BUILDFLAG(IS_ANDROID)
  PreviewManager::CreateForWebContents(&web_contents);
  PreviewManager* manager = PreviewManager::FromWebContents(&web_contents);
  CHECK(manager);
  manager->InitiatePreview(url);
#endif
}

bool Browser::ShouldUseInstancedSystemMediaControls() const {
  return is_type_app() || is_type_app_popup();
}

void Browser::DraggableRegionsChanged(
    const std::vector<blink::mojom::DraggableRegionPtr>& regions,
    content::WebContents* contents) {
  if (app_controller_) {
    app_controller_->DraggableRegionsChanged(regions, contents);
  }
}

void Browser::DidFinishNavigation(
    content::WebContents* web_contents,
    content::NavigationHandle* navigation_handle) {
  if (web_contents != tab_strip_model_->GetActiveWebContents())
    return;

  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->HasCommitted()) {
    UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
  }
}

void Browser::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}

void Browser::EnumerateDirectory(
    WebContents* web_contents,
    scoped_refptr<content::FileSelectListener> listener,
    const base::FilePath& path) {
  FileSelectHelper::EnumerateDirectory(web_contents, std::move(listener), path);
}

bool Browser::CanUseWindowingControls(
    content::RenderFrameHost* requesting_frame) {
  if (!web_app::AppBrowserController::IsWebApp(this)) {
    requesting_frame->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        "API called from something else than a web_app.");
    return false;
  }
  return true;
}

void Browser::OnCanResizeFromWebAPIChanged() {
  window_->OnCanResizeFromWebAPIChanged();
}

bool Browser::GetCanResize() {
  return window_->GetCanResize();
}

void Browser::MinimizeFromWebAPI() {
  window_->Minimize();
}

void Browser::MaximizeFromWebAPI() {
  window_->Maximize();
}

void Browser::RestoreFromWebAPI() {
  window_->Restore();
}

ui::mojom::WindowShowState Browser::GetWindowShowState() const {
  return window_->GetWindowShowState();
}

bool Browser::CanEnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame) {
  // If the tab strip isn't editable then a drag session is in progress, and it
  // is not safe to enter fullscreen. https://crbug.com/1315080
  if (!tab_strip_model_delegate_->IsTabStripEditable())
    return false;

  return exclusive_access_manager_->fullscreen_controller()
      ->CanEnterFullscreenModeForTab(requesting_frame);
}

void Browser::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  exclusive_access_manager_->fullscreen_controller()->EnterFullscreenModeForTab(
      requesting_frame, options.display_id);
}

void Browser::ExitFullscreenModeForTab(WebContents* web_contents) {
  exclusive_access_manager_->fullscreen_controller()->ExitFullscreenModeForTab(
      web_contents);
}

bool Browser::IsFullscreenForTabOrPending(const WebContents* web_contents) {
  const content::FullscreenState state = GetFullscreenState(web_contents);
  return state.target_mode == content::FullscreenMode::kContent ||
         state.target_mode == content::FullscreenMode::kPseudoContent;
}

content::FullscreenState Browser::GetFullscreenState(
    const WebContents* web_contents) const {
  return exclusive_access_manager_->fullscreen_controller()->GetFullscreenState(
      web_contents);
}

blink::mojom::DisplayMode Browser::GetDisplayMode(
    const WebContents* web_contents) {
  if (window_->IsFullscreen())
    return blink::mojom::DisplayMode::kFullscreen;

  if (is_type_picture_in_picture()) {
    return blink::mojom::DisplayMode::kPictureInPicture;
  }

  if (is_type_app() || is_type_devtools() || is_type_app_popup()) {
    if (app_controller_ && app_controller_->HasMinimalUiButtons())
      return blink::mojom::DisplayMode::kMinimalUi;

    if (app_controller_ && app_controller_->AppUsesWindowControlsOverlay() &&
        !web_contents->GetWindowsControlsOverlayRect().IsEmpty()) {
      return blink::mojom::DisplayMode::kWindowControlsOverlay;
    }

    if (app_controller_ && app_controller_->AppUsesTabbed()) {
      return blink::mojom::DisplayMode::kTabbed;
    }

    if (app_controller_ && app_controller_->AppUsesBorderlessMode() &&
        window_->IsBorderlessModeEnabled()) {
      return blink::mojom::DisplayMode::kBorderless;
    }

    return blink::mojom::DisplayMode::kStandalone;
  }

  return blink::mojom::DisplayMode::kBrowser;
}

blink::ProtocolHandlerSecurityLevel Browser::GetProtocolHandlerSecurityLevel(
    content::RenderFrameHost* requesting_frame) {
  // WARNING: This must match the logic of
  // ChromeContentRendererClient::GetProtocolHandlerSecurityLevel().
  if (requesting_frame->GetLastCommittedOrigin().scheme() ==
      chrome::kIsolatedAppScheme) {
    return blink::ProtocolHandlerSecurityLevel::kSameOrigin;
  }
  content::BrowserContext* context = requesting_frame->GetBrowserContext();
  extensions::ProcessMap* process_map = extensions::ProcessMap::Get(context);
  const Extension* owner_extension =
      extensions::ProcessManager::Get(context)->GetExtensionForRenderFrameHost(
          requesting_frame);
  if (owner_extension &&
      process_map->IsPrivilegedExtensionProcess(
          *owner_extension, requesting_frame->GetProcess()->GetID())) {
    return blink::ProtocolHandlerSecurityLevel::kExtensionFeatures;
  }
  return blink::ProtocolHandlerSecurityLevel::kStrict;
}

void Browser::RegisterProtocolHandler(
    content::RenderFrameHost* requesting_frame,
    const std::string& protocol,
    const GURL& url,
    bool user_gesture) {
  content::BrowserContext* context = requesting_frame->GetBrowserContext();
  if (context->IsOffTheRecord())
    return;

  auto* web_contents =
      content::WebContents::FromRenderFrameHost(requesting_frame);

  ProtocolHandler handler = ProtocolHandler::CreateProtocolHandler(
      protocol, url, GetProtocolHandlerSecurityLevel(requesting_frame));

  // The parameters's normalization process defined in the spec has been already
  // applied in the WebContentImpl class, so at this point it shouldn't be
  // possible to create an invalid handler.
  // https://html.spec.whatwg.org/multipage/system-state.html#normalize-protocol-handler-parameters
  DCHECK(handler.IsValid());

  custom_handlers::ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(context);
  if (registry->SilentlyHandleRegisterHandlerRequest(handler))
    return;

  // TODO(carlscab): This should probably be FromFrame() once it becomes
  // PageSpecificContentSettingsDelegate
  auto* page_content_settings_delegate =
      PageSpecificContentSettingsDelegate::FromWebContents(web_contents);
  if (!user_gesture && window_) {
    page_content_settings_delegate->set_pending_protocol_handler(handler);
    page_content_settings_delegate->set_previous_protocol_handler(
        registry->GetHandlerFor(handler.protocol()));
    window_->GetLocationBar()->UpdateContentSettingsIcons();
    return;
  }

  // Make sure content-setting icon is turned off in case the page does
  // ungestured and gestured RPH calls.
  if (window_) {
    page_content_settings_delegate->ClearPendingProtocolHandler();
    window_->GetLocationBar()->UpdateContentSettingsIcons();
  }

  if (registry->registration_mode() ==
      custom_handlers::RphRegistrationMode::kAutoAccept) {
    registry->OnAcceptRegisterProtocolHandler(handler);
    return;
  }

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  if (permission_request_manager) {
    // At this point, there will be UI presented, and running a dialog causes an
    // exit to webpage-initiated fullscreen. http://crbug.com/728276
    base::ScopedClosureRunner fullscreen_block =
        web_contents->ForSecurityDropFullscreen(
            /*display_id=*/display::kInvalidDisplayId);

    permission_request_manager->AddRequest(
        requesting_frame,
        new custom_handlers::RegisterProtocolHandlerPermissionRequest(
            registry, handler, url, std::move(fullscreen_block)));
  }
}

void Browser::UnregisterProtocolHandler(
    content::RenderFrameHost* requesting_frame,
    const std::string& protocol,
    const GURL& url,
    bool user_gesture) {
  // user_gesture will be used in case we decide to have confirmation bubble
  // for user while un-registering the handler.
  content::BrowserContext* context = requesting_frame->GetBrowserContext();
  if (context->IsOffTheRecord())
    return;

  ProtocolHandler handler = ProtocolHandler::CreateProtocolHandler(
      protocol, url, GetProtocolHandlerSecurityLevel(requesting_frame));

  custom_handlers::ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(context);
  registry->RemoveHandler(handler);
}

void Browser::FindReply(WebContents* web_contents,
                        int request_id,
                        int number_of_matches,
                        const gfx::Rect& selection_rect,
                        int active_match_ordinal,
                        bool final_update) {
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents);
  if (!find_tab_helper)
    return;

  find_tab_helper->HandleFindReply(request_id, number_of_matches,
                                   selection_rect, active_match_ordinal,
                                   final_update);
}

void Browser::RequestPointerLock(WebContents* web_contents,
                                 bool user_gesture,
                                 bool last_unlocked_by_target) {
  exclusive_access_manager_->pointer_lock_controller()->RequestToLockPointer(
      web_contents, user_gesture, last_unlocked_by_target);
}

void Browser::LostPointerLock() {
  exclusive_access_manager_->pointer_lock_controller()
      ->ExitExclusiveAccessToPreviousState();
}

bool Browser::IsWaitingForPointerLockPrompt(WebContents* web_contents) {
  return exclusive_access_manager_->pointer_lock_controller()
      ->IsWaitingForPointerLockPrompt(web_contents);
}

void Browser::RequestKeyboardLock(WebContents* web_contents,
                                  bool esc_key_locked) {
  exclusive_access_manager_->keyboard_lock_controller()->RequestKeyboardLock(
      web_contents, esc_key_locked);
}

void Browser::CancelKeyboardLockRequest(WebContents* web_contents) {
  exclusive_access_manager_->keyboard_lock_controller()
      ->CancelKeyboardLockRequest(web_contents);
}

void Browser::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  const extensions::Extension* extension =
      GetExtensionForOrigin(profile_, request.security_origin);
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), extension);
}

bool Browser::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());
  const extensions::Extension* extension =
      GetExtensionForOrigin(profile, security_origin.GetURL());
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(render_frame_host, security_origin, type,
                                   extension);
}

std::string Browser::GetTitleForMediaControls(WebContents* web_contents) {
  return app_controller_ ? app_controller_->GetTitleForMediaControls()
                         : std::string();
}

#if BUILDFLAG(ENABLE_PRINTING)
void Browser::PrintCrossProcessSubframe(
    content::WebContents* web_contents,
    const gfx::Rect& rect,
    int document_cookie,
    content::RenderFrameHost* subframe_host) const {
  auto* client = printing::PrintCompositeClient::FromWebContents(web_contents);
  if (client)
    client->PrintCrossProcessSubframe(rect, document_cookie, subframe_host);
}
#endif

#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
void Browser::CapturePaintPreviewOfSubframe(
    content::WebContents* web_contents,
    const gfx::Rect& rect,
    const base::UnguessableToken& guid,
    content::RenderFrameHost* render_frame_host) {
  auto* client =
      paint_preview::PaintPreviewClient::FromWebContents(web_contents);
  if (client)
    client->CaptureSubframePaintPreview(guid, rect, render_frame_host);
}
#endif

///////////////////////////////////////////////////////////////////////////////
// Browser, web_modal::WebContentsModalDialogManagerDelegate implementation:

void Browser::SetWebContentsBlocked(content::WebContents* web_contents,
                                    bool blocked) {
  int index = tab_strip_model_->GetIndexOfWebContents(web_contents);
  if (index == TabStripModel::kNoTab) {
    // Removal of tabs from the TabStripModel can cause observer callbacks to
    // invoke this method. The WebContents may no longer exist in the
    // TabStripModel.
    return;
  }

  // Drop HTML fullscreen to give users context for making informed decisions.
  // Skip browser-fullscreen, which is more expressly user-initiated.
  // Skip fullscreen-within-tab, which shows the browser frame.
  if (blocked && GetFullscreenState(web_contents).target_mode ==
                     content::FullscreenMode::kContent) {
    bool exit_fullscreen = true;
    if (base::FeatureList::IsEnabled(
            features::kAutomaticFullscreenContentSetting)) {
      // Skip URLs with the automatic fullscreen content setting granted.
      const GURL& url = web_contents->GetLastCommittedURL();
      const HostContentSettingsMap* const content_settings =
          HostContentSettingsMapFactory::GetForProfile(
              web_contents->GetBrowserContext());
      exit_fullscreen =
          content_settings->GetContentSetting(
              url, url, ContentSettingsType::AUTOMATIC_FULLSCREEN) !=
          CONTENT_SETTING_ALLOW;
    }
    if (exit_fullscreen) {
      web_contents->ExitFullscreen(true);
    }
  }

  tab_strip_model_->SetTabBlocked(index, blocked);

  bool browser_active = BrowserList::GetInstance()->GetLastActive() == this;
  bool contents_is_active =
      tab_strip_model_->GetActiveWebContents() == web_contents;
  // If the WebContents is foremost (the active tab in the front-most browser)
  // and is being unblocked, focus it to make sure that input works again.
  if (!blocked && contents_is_active && browser_active)
    web_contents->Focus();
}

web_modal::WebContentsModalDialogHost*
Browser::GetWebContentsModalDialogHost() {
  return window_->GetWebContentsModalDialogHost();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, BookmarkTabHelperObserver implementation:

void Browser::URLStarredChanged(content::WebContents* web_contents,
                                bool starred) {
  if (web_contents == tab_strip_model_->GetActiveWebContents())
    window_->SetStarredState(starred);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, ZoomObserver implementation:

void Browser::OnZoomControllerDestroyed(zoom::ZoomController* zoom_controller) {
  // SetAsDelegate() takes care of removing the observers.
}

void Browser::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  if (data.web_contents == tab_strip_model_->GetActiveWebContents()) {
    window_->ZoomChangedForActiveTab(data.can_show_bubble);
    // Change the zoom commands state based on the zoom state
    command_controller_->ZoomStateChanged();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, ui::SelectFileDialog::Listener implementation:

void Browser::FileSelected(const ui::SelectedFileInfo& file_info, int index) {
  // Transfer the ownership of select file dialog so that the ref count is
  // released after the function returns. This is needed because the passed-in
  // data such as |file_info| and |params| could be owned by the dialog.
  scoped_refptr<ui::SelectFileDialog> dialog = std::move(select_file_dialog_);

  profile_->set_last_selected_directory(file_info.file_path.DirName());

  GURL url =
      file_info.url.value_or(net::FilePathToFileURL(file_info.local_path));

  if (url.is_empty())
    return;

  OpenURL(OpenURLParams(url, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                        ui::PAGE_TRANSITION_TYPED, false),
          /*navigation_handle_callback=*/{});
}

void Browser::FileSelectionCanceled() {
  select_file_dialog_.reset();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, ThemeServiceObserver implementation:

void Browser::OnThemeChanged() {
  window()->UserChangedTheme(BrowserThemeChangeType::kBrowserTheme);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, translate::ContentTranslateDriver::TranslationObserver
// implementation:

void Browser::OnIsPageTranslatedChanged(content::WebContents* source) {
  DCHECK(source);
  if (tab_strip_model_->GetActiveWebContents() == source) {
    window_->SetTranslateIconToggled(
        ChromeTranslateClient::FromWebContents(source)
            ->GetLanguageState()
            .IsPageTranslated());
  }
}

void Browser::OnTranslateEnabledChanged(content::WebContents* source) {
  DCHECK(source);
  if (tab_strip_model_->GetActiveWebContents() == source)
    UpdateToolbar(false);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Command and state updating (private):

void Browser::OnTabInsertedAt(WebContents* contents, int index) {
  // If this Browser is about to be deleted, then WebContents should not be
  // added to it. This is because scheduling the delete can not be undone, and
  // proper cleanup is not done if a WebContents is added once delete it
  // scheduled (WebContents is leaked, unload handlers aren't checked...).
  // TODO(crbug.com/40064092): this should check that `is_delete_scheduled_` is
  // false.
  DUMP_WILL_BE_CHECK(!is_delete_scheduled_);

  SetAsDelegate(contents, true);

  sessions::SessionTabHelper::FromWebContents(contents)->SetWindowID(
      session_id());

  SyncHistoryWithTabs(index);

  // Make sure the loading state is updated correctly, otherwise the throbber
  // won't start if the page is loading. Note that we don't want to
  // ScheduleUIUpdate() because the tab may not have been inserted in the UI
  // yet if this function is called before TabStripModel::TabInsertedAt().
  UpdateWindowForLoadingStateChanged(contents, true);

  SessionServiceBase* service = GetAppropriateSessionServiceForProfile(this);

  if (service) {
    service->TabInserted(contents);
    int new_active_index = tab_strip_model_->active_index();
    if (index < new_active_index)
      service->SetSelectedTabInWindow(session_id(), new_active_index);
  }
}

void Browser::OnTabClosing(WebContents* contents) {
  // When this function is called |contents| has been removed from the
  // TabStripModel. Some of the following code may trigger calling to the
  // WebContentsDelegate, which is |this|, which may try to look for the
  // WebContents in the TabStripModel, and fail because the WebContents has
  // been removed. To avoid these problems the delegate is reset now.
  SetAsDelegate(contents, false);

  // Typically, ModalDialogs are closed when the WebContents is destroyed.
  // However, when the tab is being closed, we must first close the dialogs [to
  // give them an opportunity to clean up after themselves] while the state
  // associated with their tab is still valid.
  WebContentsModalDialogManager::FromWebContents(contents)->CloseAllDialogs();

  // Page load metrics need to be informed that the WebContents will soon be
  // destroyed, so that upcoming visibility changes can be ignored.
  page_load_metrics::MetricsWebContentsObserver* metrics_observer =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(contents);
  metrics_observer->WebContentsWillSoonBeDestroyed();

  exclusive_access_manager_->OnTabClosing(contents);
  SessionServiceBase* service = GetAppropriateSessionServiceForProfile(this);

  if (service)
    service->TabClosing(contents);
}

void Browser::OnTabDetached(WebContents* contents, bool was_active) {
  if (!tab_strip_model_->closing_all()) {
    SessionServiceBase* service = GetAppropriateSessionServiceIfExisting(this);
    if (service) {
      service->SetSelectedTabInWindow(session_id(),
                                      tab_strip_model_->active_index());
    }
  }

  TabDetachedAtImpl(contents, was_active, DETACH_TYPE_DETACH);

  window_->OnTabDetached(contents, was_active);
}

void Browser::OnTabDeactivated(WebContents* contents) {
  exclusive_access_manager_->OnTabDeactivated(contents);
  SearchTabHelper::FromWebContents(contents)->OnTabDeactivated();

  // Save what the user's currently typing, so it can be restored when we
  // switch back to this tab.
  window_->GetLocationBar()->SaveStateToContents(contents);
}

void Browser::OnActiveTabChanged(WebContents* old_contents,
                                 WebContents* new_contents,
                                 int index,
                                 int reason) {
  TRACE_EVENT0("ui", "Browser::OnActiveTabChanged");
// Mac correctly sets the initial background color of new tabs to the theme
// background color, so it does not need this block of code. Aura should
// implement this as well.
// https://crbug.com/719230
#if !BUILDFLAG(IS_MAC)
  // Copies the background color from an old WebContents to a new one that
  // replaces it on the screen. This allows the new WebContents to use the
  // old one's background color as the starting background color, before having
  // loaded any contents. As a result, we avoid flashing white when moving to
  // a new tab. (There is also code in RenderFrameHostManager to do something
  // similar for intra-tab navigations.)
  if (old_contents && new_contents) {
    // While GetPrimaryMainFrame() is guaranteed to return non-null, GetView()
    // is not, e.g. between WebContents creation and creation of the
    // RenderWidgetHostView.
    RenderWidgetHostView* old_view =
        old_contents->GetPrimaryMainFrame()->GetView();
    RenderWidgetHostView* new_view =
        new_contents->GetPrimaryMainFrame()->GetView();
    if (old_view && new_view)
      new_view->CopyBackgroundColorIfPresentFrom(*old_view);
  }
#endif

  base::RecordAction(UserMetricsAction("ActiveTabChanged"));

  // Update the bookmark state, since the BrowserWindow may query it during
  // OnActiveTabChanged() below.
  UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_SWITCH);

  // Let the BrowserWindow do its handling.  On e.g. views this changes the
  // focused object, which should happen before we update the toolbar below,
  // since the omnibox expects the correct element to already be focused when it
  // is updated.
  window_->OnActiveTabChanged(old_contents, new_contents, index, reason);

  exclusive_access_manager_->OnTabDetachedFromView(old_contents);

  // If we have any update pending, do it now.
  if (chrome_updater_factory_.HasWeakPtrs() && old_contents)
    ProcessPendingUIUpdates();

  // Propagate the profile to the location bar.
  UpdateToolbar((reason & CHANGE_REASON_REPLACED) == 0);

  // Update reload/stop state.
  command_controller_->LoadingStateChanged(new_contents->IsLoading(), true);

  // Update commands to reflect current state.
  command_controller_->TabStateChanged();

  // Reset the status bubble.
  StatusBubble* status_bubble = GetStatusBubble();
  if (status_bubble) {
    status_bubble->Hide();

    // Show the loading state (if any).
    status_bubble->SetStatus(
        CoreTabHelper::FromWebContents(tab_strip_model_->GetActiveWebContents())
            ->GetStatusText());
  }

  if (HasFindBarController()) {
    find_bar_controller_->ChangeWebContents(new_contents);
    find_bar_controller_->find_bar()->MoveWindowIfNecessary();
  }

  // Update sessions (selected tab index and last active time). Don't force
  // creation of sessions. If sessions doesn't exist, the change will be picked
  // up by sessions when created.
  SessionServiceBase* service = GetAppropriateSessionServiceIfExisting(this);
  if (service && !tab_strip_model_->closing_all()) {
    service->SetSelectedTabInWindow(session_id(),
                                    tab_strip_model_->active_index());
    sessions::SessionTabHelper* session_tab_helper =
        sessions::SessionTabHelper::FromWebContents(new_contents);
    service->SetLastActiveTime(session_id(), session_tab_helper->session_id(),
                               base::Time::Now());
  }

  SearchTabHelper::FromWebContents(new_contents)->OnTabActivated();
  did_active_tab_change_callback_list_.Notify(this);
}

void Browser::OnTabMoved(int from_index, int to_index) {
  DCHECK(from_index >= 0 && to_index >= 0);
  // Notify the history service.
  SyncHistoryWithTabs(std::min(from_index, to_index));
}

void Browser::OnTabReplacedAt(WebContents* old_contents,
                              WebContents* new_contents,
                              int index) {
  bool was_active = index == tab_strip_model_->active_index();
  if (was_active) {
    did_active_tab_change_callback_list_.Notify(this);
  }
  TabDetachedAtImpl(old_contents, was_active, DETACH_TYPE_REPLACE);
  exclusive_access_manager_->OnTabClosing(old_contents);
  SessionServiceBase* session_service =
      GetAppropriateSessionServiceForProfile(this);
  if (session_service)
    session_service->TabClosing(old_contents);
  OnTabInsertedAt(new_contents, index);

  if (!new_contents->GetController().IsInitialBlankNavigation()) {
    // Send out notification so that observers are updated appropriately.
    int entry_count = new_contents->GetController().GetEntryCount();
    new_contents->GetController().NotifyEntryChanged(
        new_contents->GetController().GetEntryAtIndex(entry_count - 1));
  }

  if (session_service) {
    // The new_contents may end up with a different navigation stack. Force
    // the session service to update itself.
    session_service->TabRestored(new_contents,
                                 tab_strip_model_->IsTabPinned(index));
  }
}

void Browser::OnDevToolsAvailabilityChanged() {
  for (auto& agent_host : content::DevToolsAgentHost::GetAll()) {
    if (!DevToolsWindow::AllowDevToolsFor(profile_,
                                          agent_host->GetWebContents()))
      agent_host->ForceDetachAllSessions();
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void Browser::OnLockedForOnTaskUpdated() {
  bool is_locked = IsLockedForOnTask();
  BrowserView* const browser_view = static_cast<BrowserView*>(window());
  browser_view->SetCanMinimize(!is_locked);
  browser_view->SetShowCloseButton(!is_locked);
}
#endif

///////////////////////////////////////////////////////////////////////////////
// Browser, UI update coalescing and handling (private):

void Browser::UpdateToolbar(bool should_restore_state) {
  TRACE_EVENT0("ui", "Browser::UpdateToolbar");
  window_->UpdateToolbar(should_restore_state
                             ? tab_strip_model_->GetActiveWebContents()
                             : nullptr);
}

void Browser::UpdateToolbarSecurityState() {
  TRACE_EVENT0("ui", "Browser::UpdateToolbarSecurityState");
  window_->UpdateToolbarSecurityState();
}

void Browser::ScheduleUIUpdate(WebContents* source, unsigned changed_flags) {
  DCHECK(source);
  // WebContents may in some rare cases send updates after they've been detached
  // from the tabstrip but before they are deleted, causing a potential crash if
  // we proceed. For now bail out.
  // TODO(crbug.com/40100269) Figure out a safe way to detach browser delegate
  // from WebContents when it's removed so this doesn't happen - then put a
  // DCHECK back here.
  if (tab_strip_model_->GetIndexOfWebContents(source) == TabStripModel::kNoTab)
    return;

  // Do some synchronous updates.
  if (changed_flags & content::INVALIDATE_TYPE_URL) {
    if (source == tab_strip_model_->GetActiveWebContents()) {
      // Only update the URL for the current tab. Note that we do not update
      // the navigation commands since those would have already been updated
      // synchronously by NavigationStateChanged.
      UpdateToolbar(false);
    } else {
      // Clear the saved tab state for the tab that navigated, so that we don't
      // restore any user text after the old URL has been invalidated (e.g.,
      // after a new navigation commits in that tab while unfocused).
      window_->ResetToolbarTabState(source);
    }
    changed_flags &= ~content::INVALIDATE_TYPE_URL;
  }

  if (changed_flags & content::INVALIDATE_TYPE_LOAD) {
    // Update the loading state synchronously. This is so the throbber will
    // immediately start/stop, which gives a more snappy feel. We want to do
    // this for any tab so they start & stop quickly.
    tab_strip_model_->UpdateWebContentsStateAt(
        tab_strip_model_->GetIndexOfWebContents(source),
        TabChangeType::kLoadingOnly);
    // The status bubble needs to be updated during INVALIDATE_TYPE_LOAD too,
    // but we do that asynchronously by not stripping INVALIDATE_TYPE_LOAD from
    // changed_flags.
  }

  // If the only updates were synchronously handled above, we're done.
  if (changed_flags == 0)
    return;

  // Save the dirty bits.
  scheduled_updates_[source] |= changed_flags;

  if (!chrome_updater_factory_.HasWeakPtrs()) {
    base::TimeDelta delay = update_ui_immediately_for_testing_
                                ? base::Milliseconds(0)
                                : kUIUpdateCoalescingTime;
    // No task currently scheduled, start another.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&Browser::ProcessPendingUIUpdates,
                       chrome_updater_factory_.GetWeakPtr()),
        delay);
  }
}

void Browser::ProcessPendingUIUpdates() {
#ifndef NDEBUG
  // Validate that all tabs we have pending updates for exist. This is scary
  // because the pending list must be kept in sync with any detached or
  // deleted tabs.
  for (UpdateMap::const_iterator i = scheduled_updates_.begin();
       i != scheduled_updates_.end(); ++i) {
    bool found = false;
    for (int tab = 0; tab < tab_strip_model_->count(); tab++) {
      if (tab_strip_model_->GetWebContentsAt(tab) == i->first) {
        found = true;
        break;
      }
    }
    DCHECK(found);
  }
#endif

  chrome_updater_factory_.InvalidateWeakPtrs();

  for (UpdateMap::const_iterator i = scheduled_updates_.begin();
       i != scheduled_updates_.end(); ++i) {
    // Do not dereference |contents|, it may be out-of-date!
    const WebContents* contents = i->first;
    unsigned flags = i->second;

    if (contents == tab_strip_model_->GetActiveWebContents()) {
      // Updates that only matter when the tab is selected go here.

      // Updating the URL happens synchronously in ScheduleUIUpdate.
      if (flags & content::INVALIDATE_TYPE_LOAD && GetStatusBubble()) {
        GetStatusBubble()->SetStatus(
            CoreTabHelper::FromWebContents(
                tab_strip_model_->GetActiveWebContents())
                ->GetStatusText());
      }

      if (flags &
          (content::INVALIDATE_TYPE_TAB | content::INVALIDATE_TYPE_TITLE)) {
        window_->UpdateTitleBar();
      }
    }

    // Updates that don't depend upon the selected state go here.
    if (flags & (content::INVALIDATE_TYPE_TAB | content::INVALIDATE_TYPE_TITLE |
                 content::INVALIDATE_TYPE_AUDIO)) {
      tab_strip_model_->UpdateWebContentsStateAt(
          tab_strip_model_->GetIndexOfWebContents(contents),
          TabChangeType::kAll);
    }

    // Update the bookmark bar and PWA install icon. It may happen that the tab
    // is crashed, and if so, the bookmark bar and PWA install icon should be
    // hidden.
    if (flags & content::INVALIDATE_TYPE_TAB) {
      UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
      // TODO(crbug.com/40122780): Ideally, we should simply ask the state to
      // update, and doing that in an appropriate and efficient manner.
      window()->UpdatePageActionIcon(PageActionIconType::kPwaInstall);
    }

    // We don't need to process INVALIDATE_STATE, since that's not visible.
  }

  scheduled_updates_.clear();
}

void Browser::RemoveScheduledUpdatesFor(WebContents* contents) {
  if (!contents)
    return;

  auto i = scheduled_updates_.find(contents);
  if (i != scheduled_updates_.end())
    scheduled_updates_.erase(i);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Getters for UI (private):

StatusBubble* Browser::GetStatusBubble() {
  // For kiosk and exclusive app mode we want to always hide the status bubble.
  if (IsRunningInAppMode()) {
    return nullptr;
  }

  // We hide the status bar for web apps windows as this matches native
  // experience. However, we include the status bar for 'minimal-ui' display
  // mode, as the minimal browser UI includes the status bar.
  if (web_app::AppBrowserController::IsWebApp(this) &&
      !app_controller()->HasMinimalUiButtons()) {
    return nullptr;
  }

  return window_ ? window_->GetStatusBubble() : nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Session restore functions (private):

void Browser::SyncHistoryWithTabs(int index) {
  SessionServiceBase* service = GetAppropriateSessionServiceForProfile(this);

  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(profile());

  if (!service && !session_service)
    return;

  for (int i = index; i < tab_strip_model_->count(); ++i) {
    WebContents* web_contents = tab_strip_model_->GetWebContentsAt(i);
    if (web_contents) {
      sessions::SessionTabHelper* session_tab_helper =
          sessions::SessionTabHelper::FromWebContents(web_contents);
      if (service) {
        service->SetPinnedState(session_id(), session_tab_helper->session_id(),
                                tab_strip_model_->IsTabPinned(i));
      }

      if (!IsRelevantToAppSessionService(type_) && session_service) {
        session_service->SetTabIndexInWindow(
            session_id(), session_tab_helper->session_id(), i);

        std::optional<tab_groups::TabGroupId> group_id =
            tab_strip_model_->GetTabGroupForTab(i);
        session_service->SetTabGroup(session_id(),
                                     session_tab_helper->session_id(),
                                     std::move(group_id));
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, In-progress download termination handling (private):

bool Browser::CanCloseWithInProgressDownloads() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
  // On Mac and ChromeOS, non-incognito and non-Guest downloads can still
  // continue after window is closed.
  if (!profile_->IsOffTheRecord())
    return true;
#endif

  // If we've prompted, we need to hear from the user before we
  // can close.
  if (cancel_download_confirmation_state_ != NOT_PROMPTED)
    return cancel_download_confirmation_state_ != WAITING_FOR_RESPONSE;

  int num_downloads_blocking;
  DownloadCloseType dialog_type =
      OkToCloseWithInProgressDownloads(&num_downloads_blocking);
  if (dialog_type == DownloadCloseType::kOk)
    return true;

  // Closing this window will kill some downloads; prompt to make sure
  // that's ok.
  cancel_download_confirmation_state_ = WAITING_FOR_RESPONSE;
  window_->ConfirmBrowserCloseWithPendingDownloads(
      num_downloads_blocking, dialog_type,
      base::BindOnce(&Browser::InProgressDownloadResponse,
                     weak_factory_.GetWeakPtr()));

  // Return false so the browser does not close.  We'll close if the user
  // confirms in the dialog.
  return false;
}

void Browser::InProgressDownloadResponse(bool cancel_downloads) {
  if (cancel_downloads) {
    cancel_download_confirmation_state_ = RESPONSE_RECEIVED;

    if (ShouldShowCookieMigrationNoticeForBrowser(*this)) {
      ShowCookieClearOnExitMigrationNotice(
          *this, base::BindOnce(&Browser::CookieMigrationNoticeResponse,
                                weak_factory_.GetWeakPtr()));
    } else {
      std::move(warn_before_closing_callback_)
          .Run(WarnBeforeClosingResult::kOkToClose);
    }
    return;
  }

  // Sets the confirmation state to NOT_PROMPTED so that if the user tries to
  // close again we'll show the warning again.
  cancel_download_confirmation_state_ = NOT_PROMPTED;

  // Show the download page so the user can figure-out what downloads are still
  // in-progress.
  chrome::ShowDownloads(this);

  std::move(warn_before_closing_callback_)
      .Run(WarnBeforeClosingResult::kDoNotClose);
}

void Browser::CookieMigrationNoticeResponse(bool proceed_closing) {
  std::move(warn_before_closing_callback_)
      .Run(proceed_closing ? WarnBeforeClosingResult::kOkToClose
                           : WarnBeforeClosingResult::kDoNotClose);
}

void Browser::FinishWarnBeforeClosing(WarnBeforeClosingResult result) {
  switch (result) {
    case WarnBeforeClosingResult::kOkToClose:
      chrome::CloseWindow(this);
      break;
    case WarnBeforeClosingResult::kDoNotClose:
      // Reset UnloadController::is_attempting_to_close_browser_ so that we
      // don't prompt every time any tab is closed. http://crbug.com/305516
      unload_controller_.CancelWindowClose();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Assorted utility functions (private):

void Browser::SetAsDelegate(WebContents* web_contents, bool set_delegate) {
  Browser* delegate = set_delegate ? this : nullptr;

  // WebContents...
  web_contents->SetDelegate(delegate);

  // ...and all the helpers.
  WebContentsModalDialogManager::FromWebContents(web_contents)
      ->SetDelegate(delegate);
  translate::ContentTranslateDriver* content_translate_driver =
      ChromeTranslateClient::FromWebContents(web_contents)->translate_driver();
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  if (delegate) {
    zoom_controller->AddObserver(this);
    content_translate_driver->AddTranslationObserver(this);
    BookmarkTabHelper::FromWebContents(web_contents)->AddObserver(this);
    web_contents_collection_.StartObserving(web_contents);
  } else {
    zoom_controller->RemoveObserver(this);
    content_translate_driver->RemoveTranslationObserver(this);
    BookmarkTabHelper::FromWebContents(web_contents)->RemoveObserver(this);
    web_contents_collection_.StopObserving(web_contents);
  }
}

void Browser::TabDetachedAtImpl(content::WebContents* contents,
                                bool was_active,
                                DetachType type) {
  if (type == DETACH_TYPE_DETACH) {
    // Save the current location bar state, but only if the tab being detached
    // is the selected tab.  Because saving state can conditionally revert the
    // location bar, saving the current tab's location bar state to a
    // non-selected tab can corrupt both tabs.
    if (was_active) {
      LocationBar* location_bar = window()->GetLocationBar();
      if (location_bar)
        location_bar->SaveStateToContents(contents);
    }

    if (!tab_strip_model_->closing_all())
      SyncHistoryWithTabs(0);
  }

  SetAsDelegate(contents, false);
  RemoveScheduledUpdatesFor(contents);

  if (HasFindBarController() && was_active)
    find_bar_controller_->ChangeWebContents(nullptr);
}

void Browser::UpdateWindowForLoadingStateChanged(content::WebContents* source,
                                                 bool should_show_loading_ui) {
  window_->UpdateLoadingAnimations(/* is_visible=*/!window_->IsMinimized());
  window_->UpdateTitleBar();

  WebContents* selected_contents = tab_strip_model_->GetActiveWebContents();
  if (source == selected_contents) {
    bool is_loading = source->IsLoading() && should_show_loading_ui;
    command_controller_->LoadingStateChanged(is_loading, false);
    if (GetStatusBubble()) {
      GetStatusBubble()->SetStatus(CoreTabHelper::FromWebContents(
                                       tab_strip_model_->GetActiveWebContents())
                                       ->GetStatusText());
    }
  }
}

bool Browser::NormalBrowserSupportsWindowFeature(WindowFeature feature,
                                                 bool check_can_support) const {
  bool fullscreen = ShouldHideUIForFullscreen();
  switch (feature) {
    case FEATURE_BOOKMARKBAR:
      return true;
    case FEATURE_TABSTRIP:
    case FEATURE_TOOLBAR:
    case FEATURE_LOCATIONBAR:
      return check_can_support || !fullscreen;
    case FEATURE_TITLEBAR:
    case FEATURE_NONE:
      return false;
  }
}

bool Browser::PopupBrowserSupportsWindowFeature(WindowFeature feature,
                                                bool check_can_support) const {
  bool fullscreen = ShouldHideUIForFullscreen();

  switch (feature) {
    case FEATURE_TITLEBAR:
    case FEATURE_LOCATIONBAR:
      return check_can_support || (!fullscreen && !is_trusted_source());
    case FEATURE_TABSTRIP:
    case FEATURE_TOOLBAR:
    case FEATURE_BOOKMARKBAR:
    case FEATURE_NONE:
      return false;
  }
}

bool Browser::AppPopupBrowserSupportsWindowFeature(
    WindowFeature feature,
    bool check_can_support) const {
  bool fullscreen = ShouldHideUIForFullscreen();
  switch (feature) {
    case FEATURE_TITLEBAR:
      return check_can_support || !fullscreen;
    case FEATURE_LOCATIONBAR:
      return app_controller_ && (check_can_support || !fullscreen);
    default:
      return PopupBrowserSupportsWindowFeature(feature, check_can_support);
  }
}

bool Browser::AppBrowserSupportsWindowFeature(WindowFeature feature,
                                              bool check_can_support) const {
  DCHECK(app_controller_);
  bool fullscreen = ShouldHideUIForFullscreen();
  switch (feature) {
    // Web apps should always support the toolbar, so the title/origin of the
    // current page can be shown when browsing a url that is not inside the app.
    // Note: Final determination of whether or not the toolbar is shown is made
    // by the |AppBrowserController|.
    // TODO(crbug.com/40639933): Make this control the visibility of Browser
    // Controls more generally.
    case FEATURE_TOOLBAR:
      return true;
    case FEATURE_TITLEBAR:
    // TODO(crbug.com/40639933): Make this control the visibility of
    // CustomTabBarView.
    case FEATURE_LOCATIONBAR:
      return check_can_support || !fullscreen;
    case FEATURE_TABSTRIP:
      // Even when the app has a tab strip, it should be hidden in
      // fullscreen. This is consistent with the behavior of
      // NormalBrowserSupportsWindowFeature().
      return app_controller_->has_tab_strip() &&
             (check_can_support || !fullscreen);
    case FEATURE_BOOKMARKBAR:
    case FEATURE_NONE:
      return false;
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// TODO(b/64863368): Consider Fullscreen mode.
bool Browser::CustomTabBrowserSupportsWindowFeature(
    WindowFeature feature) const {
  switch (feature) {
    case FEATURE_TOOLBAR:
      return true;
    case FEATURE_TITLEBAR:
    case FEATURE_LOCATIONBAR:
    case FEATURE_TABSTRIP:
    case FEATURE_BOOKMARKBAR:
    case FEATURE_NONE:
      return false;
  }
}
#endif

bool Browser::PictureInPictureBrowserSupportsWindowFeature(
    WindowFeature feature,
    bool check_can_support) const {
  switch (feature) {
    case FEATURE_TITLEBAR:
      return true;
    case FEATURE_LOCATIONBAR:
    case FEATURE_TABSTRIP:
    case FEATURE_TOOLBAR:
    case FEATURE_BOOKMARKBAR:
    case FEATURE_NONE:
      return false;
  }
}

bool Browser::SupportsWindowFeatureImpl(WindowFeature feature,
                                        bool check_can_support) const {
  switch (type_) {
    case TYPE_NORMAL:
      return NormalBrowserSupportsWindowFeature(feature, check_can_support);
    case TYPE_POPUP:
      return PopupBrowserSupportsWindowFeature(feature, check_can_support);
    case TYPE_APP:
      if (app_controller_)
        return AppBrowserSupportsWindowFeature(feature, check_can_support);
      // TODO(crbug.com/40639933): Change legacy apps to TYPE_APP_POPUP.
      return AppPopupBrowserSupportsWindowFeature(feature, check_can_support);
    case TYPE_DEVTOOLS:
    case TYPE_APP_POPUP:
      return AppPopupBrowserSupportsWindowFeature(feature, check_can_support);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case TYPE_CUSTOM_TAB:
      return CustomTabBrowserSupportsWindowFeature(feature);
#endif
    case TYPE_PICTURE_IN_PICTURE:
      return PictureInPictureBrowserSupportsWindowFeature(feature,
                                                          check_can_support);
  }
}

void Browser::UpdateBookmarkBarState(BookmarkBarStateChangeReason reason) {
  BookmarkBar::State state =
      ShouldShowBookmarkBar() ? BookmarkBar::SHOW : BookmarkBar::HIDDEN;

  if (state == bookmark_bar_state_)
    return;

  bookmark_bar_state_ = state;

  if (!window_)
    return;  // This is called from the constructor when window_ is NULL.

  if (reason == BOOKMARK_BAR_STATE_CHANGE_TAB_SWITCH) {
    // Don't notify BrowserWindow on a tab switch as at the time this is invoked
    // BrowserWindow hasn't yet switched tabs. The BrowserWindow implementations
    // end up querying state once they process the tab switch.
    return;
  }

  bool should_animate = reason == BOOKMARK_BAR_STATE_CHANGE_PREF_CHANGE ||
                        reason == BOOKMARK_BAR_STATE_CHANGE_FORCE_SHOW;
  window_->BookmarkBarStateChanged(
      should_animate ? BookmarkBar::ANIMATE_STATE_CHANGE
                     : BookmarkBar::DONT_ANIMATE_STATE_CHANGE);
}

bool Browser::ShouldShowBookmarkBar() const {
  if (profile_->IsGuestSession())
    return false;

  if (browser_defaults::bookmarks_enabled &&
      profile_->GetPrefs()->GetBoolean(bookmarks::prefs::kShowBookmarkBar) &&
      !ShouldHideUIForFullscreen())
    return true;

  if (force_show_bookmark_bar_flags_ != ForceShowBookmarkBarFlag::kNone) {
    return true;
  }

  WebContents* web_contents = tab_strip_model_->GetActiveWebContents();
  if (!web_contents)
    return false;

  BookmarkTabHelper* bookmark_tab_helper =
      BookmarkTabHelper::FromWebContents(web_contents);
  return bookmark_tab_helper && bookmark_tab_helper->ShouldShowBookmarkBar();
}

bool Browser::IsBrowserClosing() const {
  const BrowserList::BrowserSet& closing_browsers =
      BrowserList::GetInstance()->currently_closing_browsers();

  return base::Contains(closing_browsers, this);
}

bool Browser::ShouldStartShutdown() const {
  if (IsBrowserClosing())
    return false;

  const size_t closing_browsers_count =
      BrowserList::GetInstance()->currently_closing_browsers().size();
  return BrowserList::GetInstance()->size() == closing_browsers_count + 1u;
}

bool Browser::ShouldCreateBackgroundContents(
    content::SiteInstance* source_site_instance,
    const GURL& opener_url,
    const std::string& frame_name) {
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile_);

  if (!opener_url.is_valid() || frame_name.empty() ||
      !extension_system->is_ready())
    return false;

  // Only hosted apps have web extents, so this ensures that only hosted apps
  // can create BackgroundContents. We don't have to check for background
  // permission as that is checked in RenderMessageFilter when the CreateWindow
  // message is processed.
  const Extension* extension = extensions::ExtensionRegistry::Get(profile_)
                                   ->enabled_extensions()
                                   .GetHostedAppByURL(opener_url);
  if (!extension)
    return false;

  // No BackgroundContents allowed if BackgroundContentsService doesn't exist.
  BackgroundContentsService* service =
      BackgroundContentsServiceFactory::GetForProfile(profile_);
  if (!service)
    return false;

  // Ensure that we're trying to open this from the extension's process.
  extensions::ProcessMap* process_map = extensions::ProcessMap::Get(profile_);
  if (!source_site_instance->GetProcess() ||
      !process_map->Contains(extension->id(),
                             source_site_instance->GetProcess()->GetID())) {
    return false;
  }

  return true;
}

BackgroundContents* Browser::CreateBackgroundContents(
    content::SiteInstance* source_site_instance,
    content::RenderFrameHost* opener,
    const GURL& opener_url,
    bool is_new_browsing_instance,
    const std::string& frame_name,
    const GURL& target_url,
    const content::StoragePartitionConfig& partition_config,
    content::SessionStorageNamespace* session_storage_namespace) {
  BackgroundContentsService* service =
      BackgroundContentsServiceFactory::GetForProfile(profile_);
  const Extension* extension = extensions::ExtensionRegistry::Get(profile_)
                                   ->enabled_extensions()
                                   .GetHostedAppByURL(opener_url);
  bool allow_js_access = extensions::BackgroundInfo::AllowJSAccess(extension);
  // Only allow a single background contents per app.
  BackgroundContents* existing =
      service->GetAppBackgroundContents(extension->id());
  if (existing) {
    // For non-scriptable background contents, ignore the request altogether,
    // Note that ShouldCreateBackgroundContents() returning true will also
    // suppress creation of the normal WebContents.
    if (!allow_js_access)
      return nullptr;
    // For scriptable background pages, if one already exists, close it (even
    // if it was specified in the manifest).
    service->DeleteBackgroundContents(existing);
  }

  // Passed all the checks, so this should be created as a BackgroundContents.
  if (allow_js_access) {
    return service->CreateBackgroundContents(
        source_site_instance, opener, is_new_browsing_instance, frame_name,
        extension->id(), partition_config, session_storage_namespace);
  }

  // If script access is not allowed, create the the background contents in a
  // new SiteInstance, so that a separate process is used. We must not use any
  // of the passed-in routing IDs, as they are objects in the opener's
  // process.
  BackgroundContents* contents = service->CreateBackgroundContents(
      content::SiteInstance::Create(source_site_instance->GetBrowserContext()),
      nullptr, is_new_browsing_instance, frame_name, extension->id(),
      partition_config, session_storage_namespace);

  // When a separate process is used, the original renderer cannot access the
  // new window later, thus we need to navigate the window now.
  contents->web_contents()->GetController().LoadURL(
      target_url, content::Referrer(), ui::PAGE_TRANSITION_LINK,
      std::string());  // No extra headers.

  return contents;
}
