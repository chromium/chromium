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
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notimplemented.h"
#include "base/process/process_info.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/ai/ai_data_keyed_service.h"          // nogncheck
#include "chrome/browser/ai/ai_data_keyed_service_factory.h"  // nogncheck
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/content_settings/sound_content_setting_observer.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/devtools/devtools_toggle_action.h"
#include "chrome/browser/devtools/devtools_ui_controller.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/sessions/app_session_service.h"
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_lookup.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar_controller.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser_active_state_manager/browser_active_state_manager.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_init_state.h"
#include "chrome/browser/ui/browser_manager_service.h"
#include "chrome/browser/ui/browser_manager_service_factory.h"
#include "chrome/browser/ui/browser_tab_strip_model_delegate.h"
#include "chrome/browser/ui/browser_ui_controller/browser_ui_controller.h"
#include "chrome/browser/ui/browser_web_contents_delegate/browser_web_contents_delegate.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/dialogs/browser_dialogs.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/focus/browser_focus_controller.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/sad_tab.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_selection_state.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_modal/browser_window_modal_dialog_delegate.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/sessions/core/session_types.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_exposed_isolation_level.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/profiling.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/webplugininfo.h"
#include "net/base/filename_util.h"
#include "third_party/blink/public/mojom/frame/blocked_navigation_types.mojom.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(IS_WIN)
// windows.h must be included before shellapi.h
#include <windows.h>

#include <shellapi.h>

#include "chrome/browser/ui/view_ids.h"
#include "ui/base/win/shell.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#endif

using base::UserMetricsAction;
using content::GlobalRenderFrameHostId;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::RenderWidgetHostView;
using content::SiteInstance;
using content::WebContents;
using custom_handlers::ProtocolHandler;
using input::NativeWebKeyboardEvent;
using ui::WebDialogDelegate;
using web_modal::WebContentsModalDialogManager;

////////////////////////////////////////////////////////////////////////////////
// Browser, CreateParams:

BrowserWindowInterface* BrowserWindowInterface::FromSessionID(
    const SessionID& session_id) {
  BrowserWindowInterface* found = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetSessionID() == session_id) {
          found = browser;
        }
        return !found;
      });
  return found;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Constructors, Creation, Showing:

// static
Browser* Browser::Create(BrowserWindowCreateParams params) {
  // If this is failing, a caller is trying to create a browser when creation is
  // not possible, e.g. using the wrong profile or during shutdown. The caller
  // should handle this; see e.g. crbug.com/40154317 and crbug.com/40798999.
  CHECK_EQ(CreationStatus::kOk,
           GetBrowserWindowCreationStatusForProfile(*params.profile));

  Profile* const profile_ptr = &*params.profile;
  std::unique_ptr<Browser> browser =
      base::WrapUnique(new Browser(std::move(params)));
  Browser* const browser_ptr = browser.get();
  BrowserManagerServiceFactory::GetForProfile(profile_ptr)
      ->AddBrowser(std::move(browser));
  return browser_ptr;
}

// static
std::unique_ptr<Browser> Browser::DeprecatedCreateOwnedForTesting(
    BrowserWindowCreateParams params) {
  CHECK_IS_TEST();
  // If this is failing, a caller is trying to create a browser when creation is
  // not possible, e.g. using the wrong profile or during shutdown. The caller
  // should handle this; see e.g. crbug.com/40154317 and crbug.com/40798999.
  CHECK_EQ(CreationStatus::kOk,
           GetBrowserWindowCreationStatusForProfile(*params.profile));

  Profile* const profile_ptr = &*params.profile;
  std::unique_ptr<Browser> browser =
      base::WrapUnique(new Browser(std::move(params)));
  BrowserManagerServiceFactory::GetForProfile(profile_ptr)
      ->AddBrowserForTesting(browser.get());
  return browser;
}

Browser::Browser(BrowserWindowCreateParams params)
    : type_(params.type),
      profile_(&*params.profile),
      window_(nullptr),
      tab_strip_model_delegate_(
          std::make_unique<chrome::BrowserTabStripModelDelegate>(this)),
      tab_strip_model_(std::make_unique<TabStripModel>(
          tab_strip_model_delegate_.get(),
          profile_,
          // Tab groups are disabled for app browsers.
          (type_ == TYPE_APP || type_ == TYPE_APP_POPUP)
              ? nullptr
              : TabGroupModelFactory::GetInstance())),
      session_id_(SessionID::NewUnique()),
      keep_alive_(
          std::make_unique<ScopedKeepAlive>(KeepAliveOrigin::BROWSER,
                                            KeepAliveRestartOption::DISABLED)) {
  const bool user_gesture = params.from_user_gesture;
  const bool in_tab_dragging = params.in_tab_dragging;
  BrowserWindow* const custom_window = params.window;

  // Constructed first so that downstream features and window setup (e.g.
  // BrowserWindowFeatures and the window sizer) can query the creation and
  // initial parameters of this window.
  init_state_ = std::make_unique<BrowserInitState>(std::move(params),
                                                   unowned_user_data_host_);

  if (!profile_->IsOffTheRecord()) {
    profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
        profile_->GetOriginalProfile(), ProfileKeepAliveOrigin::kBrowserWindow);
  }

  tab_strip_model_->AddObserver(this);

  ProfileMetrics::LogProfileLaunch(profile_);

  // BrowserWindowFeatures need to be initialized before browser window
  // creation, so that the features can be used in creating components
  // in browser window.
  features_ = std::make_unique<BrowserWindowFeatures>();
  features_->Init(this);

  if (custom_window) {
    CHECK_IS_TEST() << "BrowserWindowCreateParams::window is a test-only param";
  }
  window_ =
      custom_window
          ? std::unique_ptr<BrowserWindow, BrowserWindowDeleter>(custom_window)
          : BrowserWindow::CreateBrowserWindow(this, user_gesture,
                                               in_tab_dragging);

  if (auto* const app_browser_controller =
          web_app::AppBrowserController::From(this)) {
    app_browser_controller->UpdateCustomTabBarVisibility(false);
  }

  SessionServiceBase* const session_service =
      GetAppropriateSessionServiceForSessionRestore(profile_, type_);
  if (session_service) {
    session_service->WindowOpened(this);
  }

  // Initialize the browser features that rely on the browser window now that it
  // is initialized.
  features_->InitPostWindowConstruction(this);

  // All initialization is complete; after this point, the browser should be on
  // the browser list until it is marked for destruction.
  is_initialized_ = true;

  if (profile_->IsGuestSession()) {
    base::UmaHistogramCounts100(
        "Browser.WindowCount.Guest",
        GlobalBrowserCollection::GetInstance()->GetGuestBrowserCount());
  } else if (profile_->IsIncognitoProfile()) {
    base::UmaHistogramCounts100(
        "Browser.WindowCount.Incognito",
        ProfileBrowserCollection::GetForProfile(profile_)
            ->GetOffTheRecordBrowserCount());
  }
}

Browser::~Browser() {
  if (!IsDeleteScheduled()) {
    // Guarantee the Browser has performed the necessary cleanup in the
    // `OnWindowClosing()` lifecycle hook. This may not be invoked during
    // Browser shutdown specifically in cases where clients directly reset
    // the Browser unique_ptr.
    UnloadController::From(this)->set_force_skip_warning_user_on_close(true);
    UnloadController::From(this)->OnWindowClosing();
  }

  // Stop observing notifications and destroy the tab monitor before continuing
  // with destruction. Profile destruction will unload extensions and reentrant
  // calls to Browser:: should be avoided while it is being torn down.

  window_.reset();

  // If closing the window is going to trigger a shutdown, then we need to
  // schedule all active downloads to be cancelled. This needs to be after
  // removing |this| from BrowserList so that OkToClose...() can determine
  // whether there are any other windows open for the browser.
  int num_downloads;
  if (!browser_defaults::kBrowserAliveWithNoWindows &&
      UnloadController::From(this)->OkToCloseWithInProgressDownloads(
          &num_downloads) ==
          UnloadController::DownloadCloseType::kBrowserShutdown) {
    DownloadCoreService::CancelAllDownloads(
        DownloadCoreService::CancelDownloadsTrigger::kShutdown);
  }

  // Tear down `BrowserWindowFeatures` to avoid exposing it to Browser in a
  // partially-destroyed state.
  features_.reset();

  // The tab strip should not have any tabs at this point.
  //
  // TODO(crbug.com/40887606): This DCHECK doesn't always pass.
  // TODO(crbug.com/40064092): convert this to CHECK.
  DCHECK(tab_strip_model_->empty());

  SessionServiceBase* service = GetAppropriateSessionServiceForProfile(this);

  if (service) {
    service->WindowClosed(session_id_);
  }
}

Profile* Browser::GetProfile() {
  return profile_;
}

const Profile* Browser::GetProfile() const {
  return profile_;
}

bool Browser::IsDeleteScheduled() const {
  return UnloadController::From(this)->is_delete_scheduled();
}

void Browser::OpenGURL(const GURL& gurl, WindowOpenDisposition disposition) {
  OpenURL(content::OpenURLParams(gurl, content::Referrer(), disposition,
                                 ui::PAGE_TRANSITION_LINK,
                                 /*is_renderer_initiated=*/false),
          /*navigation_handle_callback=*/{});
}

const SessionID& Browser::GetSessionID() const {
  return session_id_;
}

TabStripModel* Browser::GetTabStripModel() {
  return tab_strip_model_.get();
}

const TabStripModel* Browser::GetTabStripModel() const {
  return tab_strip_model_.get();
}

bool Browser::IsTabStripVisible() {
  return window_ && window_->IsToolbarShowing();
}

base::CallbackListSubscription Browser::RegisterBrowserDidClose(
    BrowserDidCloseCallback callback) {
  return UnloadController::From(this)->RegisterBrowserDidClose(
      std::move(callback));
}

base::CallbackListSubscription Browser::RegisterBrowserCloseCancelled(
    BrowserCloseCancelledCallback callback) {
  return UnloadController::From(this)->RegisterBrowserCloseCancelled(
      std::move(callback));
}

base::WeakPtr<BrowserWindowInterface> Browser::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
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

const BrowserWindowFeatures& Browser::GetFeatures() const {
  return *features_.get();
}

ui::UnownedUserDataHost& Browser::GetUnownedUserDataHost() {
  return unowned_user_data_host_;
}

const ui::UnownedUserDataHost& Browser::GetUnownedUserDataHost() const {
  return unowned_user_data_host_;
}

web_modal::WebContentsModalDialogHost*
Browser::GetWebContentsModalDialogHostForWindow() {
  return window_->GetWebContentsModalDialogHost();
}

web_modal::WebContentsModalDialogHost*
Browser::GetWebContentsModalDialogHostForTab(
    tabs::TabInterface* tab_interface) {
  return window_->GetWebContentsModalDialogHostFor(
      tab_interface->GetContents());
}

bool Browser::IsActive() const {
  return BrowserActiveStateManager::From(this)->IsActive();
}

base::CallbackListSubscription Browser::RegisterDidBecomeActive(
    DidBecomeActiveCallback callback) {
  return BrowserActiveStateManager::From(this)->RegisterDidBecomeActive(
      std::move(callback));
}

base::CallbackListSubscription Browser::RegisterDidBecomeInactive(
    DidBecomeInactiveCallback callback) {
  return BrowserActiveStateManager::From(this)->RegisterDidBecomeInactive(
      std::move(callback));
}

BrowserWindowInterface::Type Browser::GetType() const {
  return type_;
}

std::vector<tabs::TabInterface*> Browser::GetAllTabInterfaces() {
  std::vector<tabs::TabInterface*> results;
  results.reserve(tab_strip_model_->count());
  for (tabs::TabInterface* tab : *tab_strip_model_) {
    results.push_back(tab);
  }
  return results;
}

bool Browser::IsTabModalPopup() const {
  return is_tab_modal_popup_;
}

void Browser::SetIsTabModalPopup(bool is_tab_modal_popup,
                                 base::PassKey<internal::ScopedBrowserShower>) {
  is_tab_modal_popup_ = is_tab_modal_popup;
}

bool Browser::CreatedBySessionRestore() const {
  return BrowserInitState::From(this)->is_session_restore();
}

ui::BaseWindow* Browser::GetWindow() {
  return window_.get();
}

const ui::BaseWindow* Browser::GetWindow() const {
  return window_.get();
}

DesktopBrowserWindowCapabilities* Browser::capabilities() {
  return DesktopBrowserWindowCapabilities::From(this);
}

const DesktopBrowserWindowCapabilities* Browser::capabilities() const {
  return DesktopBrowserWindowCapabilities::From(this);
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

  return BrowserWebContentsDelegate::From(this)->OpenURLFromTab(
      nullptr, params, std::move(navigation_handle_callback));
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
      // Initialize find bar controller when tab having active find session
      // is inserted in a new window.
      find_in_page::FindTabHelper* find_tab_helper =
          find_in_page::FindTabHelper::FromWebContents(selection.new_contents);
      if (!HasFindBarController() && find_tab_helper &&
          find_tab_helper->is_find_session_active()) {
        std::ignore = CreateOrGetFindBarController();
      }
      for (const auto& contents : change.GetInsert()->contents) {
        OnTabInsertedAt(contents.contents, contents.index);
      }
      break;
    }
    case TabStripModelChange::kRemoved: {
      for (const auto& contents : change.GetRemove()->contents) {
        bool had_active_modal_dialog = false;
        if (TabRemoveReasonUtils::WillDeleteTab(contents.remove_reason)) {
          OnTabClosing(contents.tab, &had_active_modal_dialog);
        }
        OnTabDetached(contents.tab, contents.contents == selection.old_contents,
                      had_active_modal_dialog);
      }
      break;
    }
    case TabStripModelChange::kMoved:
      break;
    case TabStripModelChange::kReplaced: {
      auto* replace = change.GetReplace();
      OnTabReplacedAt(replace->old_contents, replace->new_contents,
                      replace->index);
      break;
    }
    case TabStripModelChange::kSelectionOnly:
      break;
  }

  if (!selection.active_tab_changed()) {
    return;
  }

  if (selection.old_contents) {
    OnTabDeactivated(selection.old_contents);
  }

  OnActiveTabChanged(change, selection);
}


void Browser::TabStripEmpty() {
  // Note: even though the tab strip is empty, the call to Close() may not
  // result in closing this Browser. This can happen in the case of closing
  // the last Browser with ongoing downloads.
  window_->Close();
}

void Browser::OnTabGroupFocusChanged(
    std::optional<tab_groups::TabGroupId> new_focused_group,
    std::optional<tab_groups::TabGroupId> old_focused_group) {
  if (!base::FeatureList::IsEnabled(features::kTabGroupsFocusing) ||
      tab_strip_model_->closing_all() || IsDeleteScheduled()) {
    return;
  }
  SessionService* service = SessionServiceFactory::GetForProfile(profile_);
  if (service) {
    service->AddWindowExtraData(
        session_id_, tabs::TabStripModelSelectionState::kFocusedTabGroupIdKey,
        new_focused_group.has_value() ? new_focused_group->ToString()
                                      : std::string());
  }
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
  DUMP_WILL_BE_CHECK(!IsDeleteScheduled());

  SetAsDelegate(contents, true);

  // Disable pinch zooming in undocked dev tools window due to poor UX.
  if (BrowserInitState::From(this)->create_params().app_name ==
      DevToolsWindow::kDevToolsApp) {
    contents->SetIgnoreZoomGestures(true);
  }

  // Make sure the loading state is updated correctly, otherwise the throbber
  // won't start if the page is loading. Note that we don't want to
  // ScheduleUIUpdate() because the tab may not have been inserted in the UI
  // yet if this function is called before TabStripModel::TabInsertedAt().
  BrowserUiController::From(this)->UpdateWindowForLoadingStateChanged(contents,
                                                                      true);
}

void Browser::OnTabClosing(tabs::TabInterface* tab,
                           bool* had_active_modal_dialog) {
  WebContents* contents = tab->GetContents();
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
  WebContentsModalDialogManager* dialog_manager =
      WebContentsModalDialogManager::FromWebContents(contents);
  *had_active_modal_dialog = dialog_manager && dialog_manager->IsDialogActive();
  dialog_manager->CloseAllDialogs();

  // Page load metrics need to be informed that the WebContents will soon be
  // destroyed, so that upcoming visibility changes can be ignored.
  page_load_metrics::MetricsWebContentsObserver* metrics_observer =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(contents);
  metrics_observer->WebContentsWillSoonBeDestroyed();

  ExclusiveAccessManager::From(this)->OnTabClosing(contents);
}

void Browser::OnTabDetached(tabs::TabInterface* tab,
                            bool was_active,
                            bool had_active_modal_dialog) {
  WebContents* contents = tab->GetContents();
  TabDetachedAtImpl(contents, was_active, DetachType::kDetach);

  window_->OnTabDetached(contents, was_active);

  // crbug.com/40624231: CloseAllDialogs() releases the dialog's scoped
  // input-ignore token on `contents`, but the active same-process sibling can
  // still be left without renderer focus. No active-tab change happens while
  // closing the background tab, so restore focus after detach completes.
  tabs::TabInterface* active_tab = tab_strip_model_->GetActiveTab();
  WebContents* active_contents =
      active_tab ? active_tab->GetContents() : nullptr;
  if (had_active_modal_dialog && active_tab && active_contents && !was_active &&
      active_contents != contents &&
      active_contents->GetPrimaryMainFrame()->GetProcess() ==
          tab->GetContents()->GetPrimaryMainFrame()->GetProcess()) {
    tabs::TabHandle active_tab_handle = active_tab->GetHandle();
    window_->SetFocusToLocationBar(false);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&Browser::RestoreFocusAfterTabModalPopupClose,
                       weak_factory_.GetWeakPtr(), active_tab_handle));
  }
}

void Browser::RestoreFocusAfterTabModalPopupClose(
    tabs::TabHandle active_tab_handle) {
  tabs::TabInterface* active_tab = active_tab_handle.Get();
  if (!active_tab || !window_ ||
      tab_strip_model_->GetActiveTab() != active_tab) {
    return;
  }
  BrowserFocusController::From(this)->FocusWebContentsPane();
}

void Browser::OnTabDeactivated(WebContents* contents) {
  ExclusiveAccessManager::From(this)->OnTabDeactivated(contents);
  SearchTabHelper::FromWebContents(contents)->OnTabDeactivated();

  // Save what the user's currently typing, so it can be restored when we
  // switch back to this tab.
  window_->GetLocationBar()->SaveStateToContents(contents);
}

void Browser::OnActiveTabChanged(const TabStripModelChange& change,
                                 const TabStripSelectionChange& selection) {
  TRACE_EVENT0("ui", "Browser::OnActiveTabChanged");

  // The side panel state needs to be updated on active tab changed
  // even if the tab strip is empty.
  if (change.type() != TabStripModelChange::kReplaced &&
      !tab_strip_model_->closing_all()) {
    SidePanelUI* side_panel_ui = SidePanelUI::From(this);
    if (side_panel_ui) {
      side_panel_ui->OnActiveTabChanged(
          selection.old_contents, selection.new_contents,
          /*tab_removed_for_deletion=*/
          (change.type() == TabStripModelChange::kRemoved) &&
              (TabRemoveReasonUtils::WillDeleteTab(
                  change.GetRemove()->contents[0].remove_reason)));
    }
  }

  if (tab_strip_model_->empty()) {
    return;
  }

// Mac correctly sets the initial background color of new tabs to the theme
// background color, so it does not need this block of code. Aura should
// implement this as well.
// https://crbug.com/41317454
#if !BUILDFLAG(IS_MAC)
  // Copies the background color from an old WebContents to a new one that
  // replaces it on the screen. This allows the new WebContents to use the
  // old one's background color as the starting background color, before having
  // loaded any contents. As a result, we avoid flashing white when moving to
  // a new tab. (There is also code in RenderFrameHostManager to do something
  // similar for intra-tab navigations.)
  if (selection.old_contents && selection.new_contents) {
    // While GetPrimaryMainFrame() is guaranteed to return non-null, GetView()
    // is not, e.g. between WebContents creation and creation of the
    // RenderWidgetHostView.
    RenderWidgetHostView* old_view =
        selection.old_contents->GetPrimaryMainFrame()->GetView();
    RenderWidgetHostView* new_view =
        selection.new_contents->GetPrimaryMainFrame()->GetView();
    if (old_view && new_view) {
      new_view->CopyBackgroundColorIfPresentFrom(*old_view);
    }
  }
#endif

  base::RecordAction(UserMetricsAction("ActiveTabChanged"));

  // Update the bookmark state, since the BrowserWindow may query it during
  // OnActiveTabChanged() below.
  BookmarkBarController::From(this)->UpdateBookmarkBarState(
      BookmarkBarController::StateChangeReason::kTabSwitch);

  // Let the BrowserWindow do its handling.  On e.g. views this changes the
  // focused object, which should happen before we update the toolbar below,
  // since the omnibox expects the correct element to already be focused when
  // it is updated.
  int index = selection.new_model.active().has_value()
                  ? static_cast<int>(selection.new_model.active().value())
                  : TabStripModel::kNoTab;
  window_->OnActiveTabChanged(selection.old_contents, selection.new_contents,
                              index, selection.reason);

  ExclusiveAccessManager::From(this)->OnTabDetachedFromView(
      selection.old_contents);

  // If we have any update pending, do it now.
  if (selection.old_contents) {
    BrowserUiController::From(this)->ProcessPendingUIUpdates();
  }

  // Propagate the profile to the location bar.
  BrowserUiController::From(this)->UpdateToolbar(
      (selection.reason & CHANGE_REASON_REPLACED) == 0);

  // Update reload/stop state.
  chrome::BrowserCommandController* const browser_command_controller =
      chrome::BrowserCommandController::From(this);
  browser_command_controller->LoadingStateChanged(
      selection.new_contents->IsLoading(), true);

  // Update commands to reflect current state.
  browser_command_controller->TabStateChanged();

  // Reset the status bubble.
  std::vector<StatusBubble*> status_bubbles =
      BrowserUiController::From(this)->GetStatusBubbles();
  for (StatusBubble* status_bubble : status_bubbles) {
    status_bubble->Hide();

    // Show the loading state (if any).
    if (status_bubble == status_bubbles.front()) {
      status_bubble->SetStatus(CoreTabHelper::FromWebContents(
                                   tab_strip_model_->GetActiveWebContents())
                                   ->GetStatusText());
    }
  }

  if (HasFindBarController()) {
    CreateOrGetFindBarController()->HandleActiveTabChanged(
        selection.new_contents);
  }


  SearchTabHelper::FromWebContents(selection.new_contents)->OnTabActivated();
  did_active_tab_change_callback_list_.Notify(this);
}


void Browser::OnTabReplacedAt(WebContents* old_contents,
                              WebContents* new_contents,
                              int index) {
  bool was_active = index == tab_strip_model_->active_index();
  if (was_active) {
    did_active_tab_change_callback_list_.Notify(this);
  }
  TabDetachedAtImpl(old_contents, was_active, DetachType::kReplace);
  ExclusiveAccessManager::From(this)->OnTabClosing(old_contents);
  OnTabInsertedAt(new_contents, index);

  if (!new_contents->GetController().IsInitialBlankNavigation()) {
    // Send out notification so that observers are updated appropriately.
    int entry_count = new_contents->GetController().GetEntryCount();
    new_contents->GetController().NotifyEntryChanged(
        new_contents->GetController().GetEntryAtIndex(entry_count - 1));
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Assorted utility functions (private):

void Browser::SetAsDelegate(WebContents* web_contents, bool set_delegate) {
  content::WebContentsDelegate* delegate =
      set_delegate ? BrowserWebContentsDelegate::From(this) : nullptr;

  // WebContents...
  web_contents->SetDelegate(delegate);

  // ...and all the helpers.
  // The modal dialog delegate must be set during SetAsDelegate (not via a
  // separate TabStripModelObserver) to ensure it is wired before layout
  // triggered by OnActiveTabChanged.
  web_modal::WebContentsModalDialogManager::FromWebContents(web_contents)
      ->SetDelegate(set_delegate ? BrowserWindowModalDialogDelegate::From(this)
                                 : nullptr);
  if (delegate) {
    web_contents_collection_.StartObserving(web_contents);
  } else {
    web_contents_collection_.StopObserving(web_contents);
  }
}

void Browser::TabDetachedAtImpl(content::WebContents* contents,
                                bool was_active,
                                DetachType type) {
  if (type == DetachType::kDetach) {
    // Save the current location bar state, but only if the tab being detached
    // is the selected tab.  Because saving state can conditionally revert the
    // location bar, saving the current tab's location bar state to a
    // non-selected tab can corrupt both tabs.
    if (was_active) {
      LocationBar* location_bar = window_->GetLocationBar();
      if (location_bar) {
        location_bar->SaveStateToContents(contents);
      }
    }

  }

  SetAsDelegate(contents, false);
  BrowserUiController::From(this)->RemoveScheduledUpdatesFor(contents);

  if (HasFindBarController() && was_active) {
    CreateOrGetFindBarController()->ChangeWebContents(nullptr);
  }
}

FindBarController* Browser::CreateOrGetFindBarController() {
  return GetFeatures().GetFindBarController();
}

bool Browser::HasFindBarController() {
  return GetFeatures().HasFindBarController();
}
