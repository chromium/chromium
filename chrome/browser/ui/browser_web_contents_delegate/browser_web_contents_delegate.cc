// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_web_contents_delegate/browser_web_contents_delegate.h"

#include "base/check_deref.h"
#include "base/metrics/histogram_macros.h"
#include "base/notimplemented.h"
#include "base/trace_event/trace_event.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/actor/actor_util.h"
#include "chrome/browser/background/background_contents.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/repost_form_warning_controller.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ui/blocked_content/chrome_popup_navigation_delegate.h"
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_ui_controller/browser_ui_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/keyboard_lock_controller.h"
#include "chrome/browser/ui/exclusive_access/pointer_lock_controller.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/status_bubble_views.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "components/blocked_content/list_item_position.h"
#include "components/blocked_content/popup_blocker.h"
#include "components/blocked_content/popup_tracker.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/custom_handlers/register_protocol_handler_permission_request.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/headless/console_message_logger/headless_console_message_logger.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/paint_preview/browser/paint_preview_client.h"
#include "components/permissions/permission_request_manager.h"
#include "components/printing/browser/print_composite_client.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/tabs/public/split_tab_data.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/window_container_type.mojom-shared.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "ui/base/base_window.h"
#include "url/origin.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/overscroll_pref_manager.h"
#endif  // defined(USE_AURA)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#endif  // BUILDFLAG(IS_MAC)

// Kill switch for merge safety for a fix for https://crbug.com/489205993
// TODO(crbug.com/489205993): Remove in M150 or later.
BASE_FEATURE(kBackgroundActorTaskPopupsOpenInBackground,
             base::FEATURE_ENABLED_BY_DEFAULT);

DEFINE_USER_DATA(BrowserWebContentsDelegate);

namespace {

const extensions::Extension* GetExtensionForOrigin(
    Profile* profile,
    const GURL& security_origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!security_origin.SchemeIs(extensions::kExtensionScheme)) {
    return nullptr;
  }

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          security_origin.GetHost());
  DCHECK(extension);
  return extension;
#else
  return nullptr;
#endif
}

bool ShouldCreateBackgroundContents(Profile* profile,
                                    content::SiteInstance* source_site_instance,
                                    const GURL& opener_url,
                                    const std::string& frame_name) {
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);

  if (!opener_url.is_valid() || frame_name.empty() ||
      !extension_system->is_ready()) {
    return false;
  }

  // Only hosted apps have web extents, so this ensures that only hosted apps
  // can create BackgroundContents. We don't have to check for background
  // permission as that is checked in RenderMessageFilter when the CreateWindow
  // message is processed.
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)
          ->enabled_extensions()
          .GetHostedAppByURL(opener_url);
  if (!extension) {
    return false;
  }

  // No BackgroundContents allowed if BackgroundContentsService doesn't exist.
  BackgroundContentsService* service =
      BackgroundContentsServiceFactory::GetForProfile(profile);
  if (!service) {
    return false;
  }

  // Ensure that we're trying to open this from the extension's process.
  extensions::ProcessMap* process_map = extensions::ProcessMap::Get(profile);
  if (!source_site_instance->HasProcess() ||
      !process_map->Contains(extension->id(),
                             source_site_instance->GetProcess()->GetID())) {
    return false;
  }

  return true;
}

BackgroundContents* CreateBackgroundContents(
    Profile* profile,
    content::SiteInstance* source_site_instance,
    content::RenderFrameHost* opener,
    const GURL& opener_url,
    bool is_new_browsing_instance,
    const std::string& frame_name,
    const GURL& target_url,
    const content::StoragePartitionConfig& partition_config,
    content::SessionStorageNamespace* session_storage_namespace) {
  BackgroundContentsService* service =
      BackgroundContentsServiceFactory::GetForProfile(profile);
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)
          ->enabled_extensions()
          .GetHostedAppByURL(opener_url);

  if (!extension) {
    return nullptr;
  }

  // Background content URLs need to be part of the extension's web extent or be
  // blank.
  if (base::FeatureList::IsEnabled(
          extensions_features::kBlockBackgroundContentsOffExtentNavigation) &&
      !target_url.is_empty() && !target_url.SchemeIs(url::kAboutScheme) &&
      !extension->web_extent().MatchesURL(target_url)) {
    return nullptr;
  }
  bool allow_js_access = extensions::BackgroundInfo::AllowJSAccess(extension);
  // Only allow a single background contents per app.
  BackgroundContents* existing =
      service->GetAppBackgroundContents(extension->id());
  if (existing) {
    // For non-scriptable background contents, ignore the request altogether,
    // Note that ShouldCreateBackgroundContents() returning true will also
    // suppress creation of the normal WebContents.
    if (!allow_js_access) {
      return nullptr;
    }
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
  content::NavigationController::LoadURLParams params(target_url);
  params.is_renderer_initiated = true;
  if (opener) {
    params.initiator_origin = opener->GetLastCommittedOrigin();
    params.initiator_process_id = opener->GetProcess()->GetID();
  } else {
    params.initiator_origin = url::Origin::Create(opener_url);
  }
  params.source_site_instance = source_site_instance;
  contents->web_contents()->GetController().LoadURLWithParams(params);

  return contents;
}

}  // namespace

BrowserWebContentsDelegate::BrowserWebContentsDelegate(
    BrowserWindowInterface* browser,
    ExclusiveAccessManager& exclusive_access_manager,
    chrome::BrowserCommandController& command_controller,
    UnloadController& unload_controller,
    web_app::AppBrowserController* app_browser_controller,
    BrowserWindow& window,
    DesktopBrowserWindowCapabilities& capabilities,
    BrowserUiController& browser_ui_controller)
    : exclusive_access_manager_(exclusive_access_manager),
      command_controller_(command_controller),
      unload_controller_(unload_controller),
      app_browser_controller_(app_browser_controller),
      window_(window),
      capabilities_(capabilities),
      browser_ui_controller_(browser_ui_controller),
      browser_(*browser),
      scoped_data_holder_(browser->GetUnownedUserDataHost(), *this) {}

BrowserWebContentsDelegate* BrowserWebContentsDelegate::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

const BrowserWebContentsDelegate* BrowserWebContentsDelegate::From(
    const BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

BrowserWebContentsDelegate::~BrowserWebContentsDelegate() = default;

content::PictureInPictureResult
BrowserWebContentsDelegate::EnterPictureInPicture(
    content::WebContents* web_contents) {
  return PictureInPictureWindowManager::GetInstance()
      ->EnterVideoPictureInPicture(web_contents);
}

void BrowserWebContentsDelegate::ExitPictureInPicture() {
  PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
}

content::KeyboardEventProcessingResult
BrowserWebContentsDelegate::PreHandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  // Forward keyboard events to the manager for fullscreen / mouse lock. This
  // may consume the event (e.g., Esc exits fullscreen mode).
  // TODO(koz): Write a test for this http://crbug.com/40647724.
  if (exclusive_access_manager_->HandleUserKeyEvent(event)) {
    return content::KeyboardEventProcessingResult::HANDLED;
  }

  return window_->PreHandleKeyboardEvent(event);
}

bool BrowserWebContentsDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  DevToolsWindow* devtools_window =
      DevToolsWindow::GetInstanceForInspectedWebContents(source);
  return (devtools_window && devtools_window->ForwardKeyboardEvent(event)) ||
         window_->HandleKeyboardEvent(event);
}

void BrowserWebContentsDelegate::RequestPointerLock(
    content::WebContents* web_contents,
    bool user_gesture,
    bool last_unlocked_by_target) {
  exclusive_access_manager_->pointer_lock_controller()->RequestToLockPointer(
      web_contents, user_gesture, last_unlocked_by_target);
}

void BrowserWebContentsDelegate::LostPointerLock() {
  exclusive_access_manager_->pointer_lock_controller()
      ->ExitExclusiveAccessToPreviousState();
}

bool BrowserWebContentsDelegate::IsWaitingForPointerLockPrompt(
    content::WebContents* web_contents) {
  return exclusive_access_manager_->pointer_lock_controller()
      ->IsWaitingForPointerLockPrompt(web_contents);
}

bool BrowserWebContentsDelegate::AllowKeyboardLockForInnerContents(
    content::WebContents* web_contents) {
  return capabilities_->AllowKeyboardLockForInnerContents(web_contents);
}

void BrowserWebContentsDelegate::RequestKeyboardLock(
    content::WebContents* web_contents,
    bool esc_key_locked) {
  exclusive_access_manager_->keyboard_lock_controller()->RequestKeyboardLock(
      web_contents, esc_key_locked);
}

void BrowserWebContentsDelegate::CancelKeyboardLockRequest(
    content::WebContents* web_contents) {
  exclusive_access_manager_->keyboard_lock_controller()
      ->CancelKeyboardLockRequest(web_contents);
}

void BrowserWebContentsDelegate::SetTopControlsShownRatio(
    content::WebContents* web_contents,
    float ratio) {
  window_->SetTopControlsShownRatio(web_contents, ratio);
}

int BrowserWebContentsDelegate::GetTopControlsHeight() {
  return window_->GetTopControlsHeight();
}

bool BrowserWebContentsDelegate::DoBrowserControlsShrinkRendererSize(
    content::WebContents* contents) {
  return window_->DoBrowserControlsShrinkRendererSize(contents);
}

int BrowserWebContentsDelegate::GetVirtualKeyboardHeight(
    content::WebContents* contents) {
  // This API is currently only used by View Transitions when the virtual
  // keyboard resizes content.  On desktop platforms, the virtual keyboard can
  // only inset the visual viewport so it shouldn't ever be called.
  NOTIMPLEMENTED();
  return 0;
}

void BrowserWebContentsDelegate::SetTopControlsGestureScrollInProgress(
    bool in_progress) {
  window_->SetTopControlsGestureScrollInProgress(in_progress);
}

bool BrowserWebContentsDelegate::CanOverscrollContent() {
#if defined(USE_AURA)
  return browser_->GetFeatures()
      .overscroll_pref_manager()
      ->CanOverscrollContent();
#else
  return false;
#endif
}

bool BrowserWebContentsDelegate::ShouldPreserveAbortedURLs(
    content::WebContents* source) {
  // Allow failed URLs to stick around in the omnibox on the NTP, but not when
  // other pages have committed.
  Profile* profile = Profile::FromBrowserContext(source->GetBrowserContext());
  if (!profile || !source->GetController().GetLastCommittedEntry()) {
    return false;
  }
  GURL committed_url(source->GetController().GetLastCommittedEntry()->GetURL());
  return search::IsNTPOrRelatedURL(committed_url, profile);
}

void BrowserWebContentsDelegate::SetFocusToLocationBar() {
  // Two differences between this and FocusLocationBar():
  // (1) This doesn't get recorded in user metrics, since it's called
  //     internally.
  // (2) This is called with |is_user_initiated| == false, because this is a
  //     renderer initiated focus (this method is a WebContentsDelegate
  //     override).
  window_->SetFocusToLocationBar(false);
}

void BrowserWebContentsDelegate::PreHandleDragUpdate(
    const content::DropData& drop_data,
    const gfx::PointF& client_pt) {
  window_->PreHandleDragUpdate(drop_data, client_pt);
}

void BrowserWebContentsDelegate::PreHandleDragExit() {
  window_->PreHandleDragExit();
}

void BrowserWebContentsDelegate::HandleDragEnded() {
  window_->HandleDragEnded();
}

bool BrowserWebContentsDelegate::CanDragEnter(
    content::WebContents* source,
    const content::DropData& data,
    blink::DragOperationsMask operations_allowed) {
#if BUILDFLAG(IS_CHROMEOS)
  // Disallow drag-and-drop navigation for Settings windows which do not support
  // external navigation.
  if ((operations_allowed & blink::kDragOperationLink) &&
      chrome::SettingsWindowManager::GetInstance()->IsSettingsBrowser(
          browser_->GetBrowserForMigrationOnly())) {
    return false;
  }
#endif
  return true;
}

void BrowserWebContentsDelegate::CreateSmsPrompt(
    content::RenderFrameHost*,
    const std::vector<url::Origin>&,
    const std::string& one_time_code,
    base::OnceClosure on_confirm,
    base::OnceClosure on_cancel) {
  // TODO(crbug.com/40103792): implementation left pending deliberately.
  std::move(on_confirm).Run();
}

bool BrowserWebContentsDelegate::ShouldAllowRunningInsecureContent(
    content::WebContents* web_contents,
    bool allowed_per_prefs,
    const url::Origin& origin,
    const GURL& resource_url) {
  // Note: this implementation is a mirror of
  // ContentSettingsObserver::allowRunningInsecureContent.
  if (allowed_per_prefs) {
    return true;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile);
  return content_settings->GetContentSetting(
             web_contents->GetLastCommittedURL(), GURL(),
             ContentSettingsType::MIXEDSCRIPT) == CONTENT_SETTING_ALLOW;
}

void BrowserWebContentsDelegate::OnDidBlockNavigation(
    content::WebContents* web_contents,
    const GURL& blocked_url,
    const GURL& initiator_url,
    const url::Origin& initiator_origin,
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
      framebust_helper->AddBlockedUrl(blocked_url, initiator_origin,
                                      base::BindOnce(on_click));
    }
  }
}

bool BrowserWebContentsDelegate::IsBackForwardCacheSupported(
    content::WebContents& web_contents) {
  return true;
}

content::PreloadingEligibility
BrowserWebContentsDelegate::IsPrerender2Supported(
    content::WebContents& web_contents,
    content::PreloadingTriggerType trigger_type) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents.GetBrowserContext());
  return prefetch::IsSomePreloadingEnabled(*profile->GetPrefs());
}

bool BrowserWebContentsDelegate::ShouldShowStaleContentOnEviction(
    content::WebContents* source) {
#if BUILDFLAG(IS_CHROMEOS)
  return source == browser_->GetTabStripModel()->GetActiveWebContents();
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

content::WebContents* BrowserWebContentsDelegate::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  TRACE_EVENT1("navigation", "BrowserWebContentsDelegate::OpenURLFromTab",
               "source", source);
#if DCHECK_IS_ON()
  DCHECK(params.Valid());
#endif

  if ((browser_->GetType() == BrowserWindowInterface::Type::TYPE_DEVTOOLS)) {
    DevToolsWindow* window = DevToolsWindow::AsDevToolsWindow(source);
    DCHECK(window);
    return window->OpenURLFromTab(source, params,
                                  std::move(navigation_handle_callback));
  }

  // If the source is already split, navigate the other pane instead of
  // creating a new tab. Return |source| so that WebContentsImpl::OpenURL()
  // sees new_contents == this and skips the DidOpenRequestedURL notification,
  // which is only meant for newly created WebContents.
  if (params.disposition == WindowOpenDisposition::NEW_SPLIT_VIEW && source) {
    tabs::TabInterface* const source_tab =
        tabs::TabInterface::MaybeGetFromContents(source);
    if (source_tab && source_tab->IsSplit()) {
      const split_tabs::SplitTabId split_id = source_tab->GetSplit().value();
      for (tabs::TabInterface* tab :
           browser_->GetTabStripModel()->GetSplitData(split_id)->ListTabs()) {
        if (tab != source_tab) {
          content::NavigationController::LoadURLParams load_params(params);
          tab->GetContents()->GetController().LoadURLWithParams(load_params);
          return source;
        }
      }
    }
  }

  NavigateParams nav_params(&*browser_, params.url, params.transition);
  nav_params.FillNavigateParamsFromOpenURLParams(params);
  nav_params.source_contents = source;
  nav_params.tabstrip_add_types = AddTabTypes::ADD_NONE;
  if (params.user_gesture) {
    nav_params.window_action = NavigateParams::WindowAction::kShowWindow;
  }
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
    if (!popup_delegate) {
      return nullptr;
    }
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

  TRACE_EVENT_INSTANT(
      "navigation", "BrowserWebContentsDelegate::OpenURLFromTab_Result",
      "navigated_or_inserted_contents", navigated_or_inserted_contents);

  return navigated_or_inserted_contents;
}

void BrowserWebContentsDelegate::NavigationStateChanged(
    content::WebContents* source,
    content::InvalidateTypes changed_flags) {
  // If we're shutting down we should refuse to process this message.
  // See crbug.com/40827720; it's possible that a WebContents sends navigation
  // state messages while destructing during browser tear-down. Ironically we
  // can't use IsShuttingDown() because by this point the browser is entirely
  // removed from the browser list.
  if (browser_->IsDeleteScheduled()) {
    return;
  }

  // Only update the UI when something visible has changed.
  if (changed_flags) {
    browser_ui_controller_->ScheduleUIUpdate(source, changed_flags);
  }

  // We can synchronously update commands since they will only change once per
  // navigation, so we don't have to worry about flickering. We do, however,
  // need to update the command state early on load to always present usable
  // actions in the face of slow-to-commit pages.
  if (changed_flags &
      (content::INVALIDATE_TYPE_URL | content::INVALIDATE_TYPE_LOAD |
       content::INVALIDATE_TYPE_TAB)) {
    command_controller_->TabStateChanged();
  }

  if (app_browser_controller_) {
    app_browser_controller_->UpdateCustomTabBarVisibility(true);
  }
}

void BrowserWebContentsDelegate::VisibleSecurityStateChanged(
    content::WebContents* source) {
  // When the current tab's security state changes, we need to update the URL
  // bar to reflect the new state.
  DCHECK(source);
  if (browser_->GetTabStripModel()->GetActiveWebContents() == source) {
    window_->UpdateToolbarSecurityState();

    if (app_browser_controller_) {
      app_browser_controller_->UpdateCustomTabBarVisibility(true);
    }
  }
}

content::WebContents* BrowserWebContentsDelegate::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
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
  display::Screen* screen = display::Screen::Get();
  bool targeting_different_display =
      screen && source && source->GetContentNativeView() &&
      screen->GetDisplayNearestView(source->GetContentNativeView()) !=
          screen->GetDisplayMatching(window_features.bounds);
  if (!app_browser_controller_ &&
      disposition == WindowOpenDisposition::NEW_POPUP &&
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
  NavigateParams::WindowAction window_action =
      NavigateParams::WindowAction::kShowWindow;
  if (disposition == WindowOpenDisposition::NEW_POPUP &&
      fullscreen_controller->GetFullscreenState(source).target_mode ==
          content::FullscreenMode::kContent) {
    window_action = NavigateParams::WindowAction::kShowWindowInactive;
    fullscreen_controller->FullscreenTabOpeningPopup(source,
                                                     new_contents.get());
    // Defer popup creation if the opener has a fullscreen transition in
    // progress. This works around a defect on Mac where separate displays
    // cannot switch their independent spaces simultaneously
    // (crbug.com/40221919)
    auto web_contents_creation_callback = base::BindOnce(
        &chrome::AddWebContents, &*browser_, source, std::move(new_contents),
        target_url, disposition, window_features, window_action, user_gesture);
    fullscreen_controller->RunOrDeferUntilTransitionIsComplete(base::BindOnce(
        base::IgnoreResult(std::move(web_contents_creation_callback))));
    return nullptr;
  }

  // If a backgrounded actor task triggered a new tab/popup, don't interrupt the
  // user.
  if (base::FeatureList::IsEnabled(
          kBackgroundActorTaskPopupsOpenInBackground) &&
      source && actor::IsRunningBackgroundActorTask(*source)) {
    if (disposition == WindowOpenDisposition::NEW_POPUP) {
      window_action = NavigateParams::WindowAction::kShowWindowInactive;
    } else if (disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB) {
      disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
    }
  }

  return chrome::AddWebContents(&*browser_, source, std::move(new_contents),
                                target_url, disposition, window_features,
                                window_action, user_gesture);
}

void BrowserWebContentsDelegate::ActivateContents(
    content::WebContents* contents) {
  // A WebContents can ask to activate after it's been removed from the
  // TabStripModel. See https://crbug.com/40679349
  int index = browser_->GetTabStripModel()->GetIndexOfWebContents(contents);
  if (index == TabStripModel::kNoTab) {
    return;
  }
  browser_->GetTabStripModel()->ActivateTabAt(index);
  window_->Activate();
}

bool BrowserWebContentsDelegate::IsContentsActive(
    content::WebContents* contents) {
  return browser_->GetTabStripModel()->GetActiveWebContents() == contents;
}

void BrowserWebContentsDelegate::LoadingStateChanged(
    content::WebContents* source,
    bool should_show_loading_ui) {
  browser_ui_controller_->ScheduleUIUpdate(source,
                                           content::INVALIDATE_TYPE_LOAD);
  browser_ui_controller_->UpdateWindowForLoadingStateChanged(
      source, should_show_loading_ui);
}

void BrowserWebContentsDelegate::CloseContents(content::WebContents* source) {
  if (unload_controller_->CanCloseContents(source)) {
    chrome::CloseWebContents(browser_->GetBrowserForMigrationOnly(), source,
                             true);
  }
}

void BrowserWebContentsDelegate::SetContentsBounds(content::WebContents* source,
                                                   const gfx::Rect& bounds) {
  if ((browser_->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL)) {
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

void BrowserWebContentsDelegate::UpdateTargetURL(content::WebContents* source,
                                                 const GURL& url) {
  std::vector<StatusBubble*> status_bubbles =
      browser_ui_controller_->GetStatusBubbles();
  for (StatusBubble* status_bubble : status_bubbles) {
    StatusBubbleViews* status_bubble_views =
        static_cast<StatusBubbleViews*>(status_bubble);
    ContentsWebView* anchor =
        static_cast<ContentsWebView*>(status_bubble_views->base_view());
    if (source == anchor->GetWebContents()) {
      status_bubble->SetURL(url);
      break;
    }
  }
}

void BrowserWebContentsDelegate::ContentsMouseEvent(
    content::WebContents* source,
    const ui::Event& event) {
  const ui::EventType type = event.type();
  const bool exited = type == ui::EventType::kMouseExited;
  // Disregard synthesized events, and mouse enter and exit, which may occur
  // without explicit user input events during window state changes.
  if (type != ui::EventType::kMouseEntered && !exited &&
      !event.IsSynthesized()) {
    exclusive_access_manager_->OnUserInput();
  }

  // Mouse motion events update the status bubble, if it exists.
  std::vector<StatusBubble*> status_bubbles =
      browser_ui_controller_->GetStatusBubbles();
  for (StatusBubble* status_bubble : status_bubbles) {
    StatusBubbleViews* status_bubble_views =
        static_cast<StatusBubbleViews*>(status_bubble);
    ContentsWebView* anchor =
        static_cast<ContentsWebView*>(status_bubble_views->base_view());
    if (source == anchor->GetWebContents() &&
        (type == ui::EventType::kMouseMoved || exited)) {
      status_bubble->MouseMoved(exited);
      if (exited) {
        status_bubble->SetURL(GURL());
      }
      break;
    }
  }
}

void BrowserWebContentsDelegate::ContentsZoomChange(bool zoom_in) {
  chrome::ExecuteCommand(&*browser_, zoom_in ? IDC_ZOOM_PLUS : IDC_ZOOM_MINUS);
}

bool BrowserWebContentsDelegate::TakeFocus(content::WebContents* source,
                                           bool reverse) {
  return false;
}

bool BrowserWebContentsDelegate::DidAddMessageToConsole(
    content::WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
  static bool is_headless_mode = headless::IsHeadlessMode();
  if (is_headless_mode) {
    const bool is_builtin_component = !!source->GetWebUI();
    headless::LogConsoleMessage(log_level, message, line_no,
                                is_builtin_component, source_id);
    return true;
  }
  return false;
}

void BrowserWebContentsDelegate::BeforeUnloadFired(
    content::WebContents* web_contents,
    bool proceed,
    bool* proceed_to_fire_unload) {
  unload_controller_->BeforeUnloadFired(web_contents, proceed,
                                        proceed_to_fire_unload);
}

bool BrowserWebContentsDelegate::ShouldFocusLocationBarByDefault(
    content::WebContents* source) {
  // Navigations in background tabs shouldn't change the focus state of the
  // omnibox, since it's associated with the foreground tab.
  if (source != browser_->GetTabStripModel()->GetActiveWebContents()) {
    return false;
  }

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

    if (virtual_url.SchemeIs(content::kViewSourceScheme)) {
      return false;
    }

    if ((url.SchemeIs(content::kChromeUIScheme) &&
         url.host() == chrome::kChromeUINewTabHost) ||
        (virtual_url.SchemeIs(content::kChromeUIScheme) &&
         virtual_url.host() == chrome::kChromeUINewTabHost)) {
      return true;
    }

    if (url.spec() == chrome::kChromeUISplitViewNewTabPageURL) {
      return true;
    }
  }

  return search::NavEntryIsInstantNTP(source, entry);
}

bool BrowserWebContentsDelegate::ShouldFocusPageAfterCrash(
    content::WebContents* source) {
  // Focus only the active page when reloading after a crash, otherwise
  // return false. This is to ensure background reloads via hovercard
  // don't end up causing a focus loss which results in its dismissal.
  return source == browser_->GetTabStripModel()->GetActiveWebContents();
}

void BrowserWebContentsDelegate::ShowRepostFormWarningDialog(
    content::WebContents* source) {
  TabModalConfirmDialog::Create(
      std::make_unique<RepostFormWarningController>(source), source);
}

std::unique_ptr<content::EyeDropper> BrowserWebContentsDelegate::OpenEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  return window_->OpenEyeDropper(frame, listener);
}

bool BrowserWebContentsDelegate::ShouldUseInstancedSystemMediaControls() const {
  return (browser_->GetType() == BrowserWindowInterface::Type::TYPE_APP) ||
         (browser_->GetType() == BrowserWindowInterface::Type::TYPE_APP_POPUP);
}

void BrowserWebContentsDelegate::DraggableRegionsChanged(
    const std::vector<blink::mojom::DraggableRegionPtr>& regions,
    content::WebContents* contents) {
  if (app_browser_controller_) {
    app_browser_controller_->DraggableRegionsChanged(regions, contents);
  }
}

std::vector<blink::mojom::RelatedApplicationPtr>
BrowserWebContentsDelegate::GetSavedRelatedApplications(
    content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  CHECK(profile);
  if (!web_app::AreWebAppsEnabled(profile)) {
    return {};
  }
  const webapps::AppId* app_id =
      web_app::WebAppTabHelper::GetAppId(web_contents);
  if (!app_id || app_id->empty()) {
    return {};
  }
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile);
  CHECK(provider);
  std::vector<blink::Manifest::RelatedApplication> saved_related_apps =
      provider->registrar_unsafe().GetRelatedApplications(*app_id);
  std::vector<blink::mojom::RelatedApplicationPtr> related_apps_ptr;
  for (const auto& app : saved_related_apps) {
    auto related_app = blink::mojom::RelatedApplication::New();
    related_app->platform =
        base::UTF16ToUTF8(app.platform.value_or(std::u16string()));
    related_app->id = base::UTF16ToUTF8(app.id.value_or(std::u16string()));
    if (!app.url.is_empty()) {
      related_app->url = app.url.spec();
    }
    related_apps_ptr.push_back(std::move(related_app));
  }
  return related_apps_ptr;
}

content::WebContents* BrowserWebContentsDelegate::GetResponsibleWebContents(
    content::WebContents* web_contents) {
  // Tabs are the proper choice for modal scope.
  return web_contents;
}

std::optional<gfx::Rect> BrowserWebContentsDelegate::GetWindowBoundsInScreen() {
  // Note that `GetBounds` here returns the screen coordinate bounds
  // from the browser widget. This is not to be confused with
  // `views::View::bounds()` which returns parent-relative bounds.
  return window_->GetBounds();
}

bool BrowserWebContentsDelegate::IsWebContentsCreationOverridden(
    content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  if (actor::HasActorTaskPreventingNewWebContents(opener)) {
    // If an ExecutionEngine is acting on the opener, prevent it from creating a
    // new WebContents. We'll instead force the navigation to happen in the same
    // tab. Note, we do this even if the task isn't active (e.g. paused) so that
    // a user action on behalf of the actor has the same behavior since the
    // resumed task will still be fixed to the tab.

    // However, if the opener is sandboxed and restricted from top-level
    // navigation, we cannot force a same-tab redirection as it would violate
    // the sandbox. Instead, we decline to override creation, allowing the
    // browser to safely open a new popup window (since kPopups is allowed).
    if (opener &&
        opener->IsSandboxed(network::mojom::WebSandboxFlags::kTopNavigation)) {
      return false;
    }
    return true;
  }

  return (window_container_type ==
              content::mojom::WindowContainerType::BACKGROUND &&
          ShouldCreateBackgroundContents(browser_->GetProfile(),
                                         source_site_instance, opener_url,
                                         frame_name));
}

content::WebContents* BrowserWebContentsDelegate::CreateCustomWebContents(
    content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance,
    bool is_new_browsing_instance,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    const content::StoragePartitionConfig& partition_config,
    content::SessionStorageNamespace* session_storage_namespace) {
  if (auto* opener_contents = content::WebContents::FromRenderFrameHost(opener);
      actor::HasActorTaskPreventingNewWebContents(opener)) {
    // If an ExecutionEngine is acting on the opener, we force the navigation
    // to happen in the same tab.
    content::NavigationController::LoadURLParams params(target_url);
    params.initiator_frame_token = opener->GetFrameToken();
    params.initiator_process_id = opener->GetProcess()->GetID();
    params.initiator_origin = opener->GetLastCommittedOrigin();
    params.source_site_instance = source_site_instance;
    params.transition_type = ui::PAGE_TRANSITION_LINK;
    params.is_renderer_initiated = true;
    opener_contents->GetController().LoadURLWithParams(params);
    VLOG(1) << "Actor treated window open as same tab navigation. "
            << target_url;
    return nullptr;
  }

  BackgroundContents* background_contents = CreateBackgroundContents(
      browser_->GetProfile(), source_site_instance, opener, opener_url,
      is_new_browsing_instance, frame_name, target_url, partition_config,
      session_storage_namespace);
  if (background_contents) {
    return background_contents->web_contents();
  }
  return nullptr;
}

void BrowserWebContentsDelegate::WebContentsCreated(
    content::WebContents* source_contents,
    const content::GlobalRenderFrameHostId& opener_id,
    const std::string& frame_name,
    const GURL& target_url,
    content::WebContents* new_contents) {
  // Note: Consult owners before adding new code here.
  // This method is called from WebContentsImpl::CreateNewWindow() for a created
  // `new_contents`. This occurs before ownership of `new_contents` is
  // transferred to Browser and `new_contents` is added to a TabModel. Tab
  // specific initialization should be performed by TabModel and not added here.

  // SafeBrowsingNavigationObserver relies on recording a precise sequence of
  // navigation events, with tabs tracked via their SessionID, managed by
  // SessionTabHelper. The current safe browsing implementation requires
  // tracking new contents from the moment of creation, at which point TabModel
  // and tab helpers have not yet been initialized for `new_contents`.
  // Explicitly instantiate the SessionTabHelper here to ensure SessionIDs are
  // available when needed.
  // TODO(crbug.com/362038317): Once SafeBrowsingNavigationObserver is updated
  // to track `new_contents` after it is added to its TabModel this override can
  // be removed.
  CreateSessionServiceTabHelper(new_contents);
}

void BrowserWebContentsDelegate::RendererUnresponsive(
    content::WebContents* source,
    content::RenderWidgetHost* render_widget_host,
    base::RepeatingClosure hang_monitor_restarter) {
  // Don't show the page hung dialog when a HTML popup hangs because
  // the dialog will take the focus and immediately close the popup.
  content::RenderWidgetHostView* view = render_widget_host->GetView();
  if (view && !render_widget_host->GetView()->IsHTMLFormPopup()) {
    TabDialogs::FromWebContents(source)->ShowHungRendererDialog(
        render_widget_host, std::move(hang_monitor_restarter));
  }
}

void BrowserWebContentsDelegate::RendererResponsive(
    content::WebContents* source,
    content::RenderWidgetHost* render_widget_host) {
  content::RenderWidgetHostView* view = render_widget_host->GetView();
  if (view && !render_widget_host->GetView()->IsHTMLFormPopup()) {
    TabDialogs::FromWebContents(source)->HideHungRendererDialog(
        render_widget_host);
  }
}

content::JavaScriptDialogManager*
BrowserWebContentsDelegate::GetJavaScriptDialogManager(
    content::WebContents* source) {
  return javascript_dialogs::TabModalDialogManager::FromWebContents(source);
}

bool BrowserWebContentsDelegate::GuestSaveFrame(
    content::WebContents* guest_web_contents) {
  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromWebContents(guest_web_contents);
  return guest_view && guest_view->PluginDoSave();
}

void BrowserWebContentsDelegate::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}

void BrowserWebContentsDelegate::EnumerateDirectory(
    content::WebContents* web_contents,
    scoped_refptr<content::FileSelectListener> listener,
    const base::FilePath& path) {
  FileSelectHelper::EnumerateDirectory(web_contents, std::move(listener), path);
}

bool BrowserWebContentsDelegate::GetCanResize() {
  return window_->GetCanResize();
}

bool BrowserWebContentsDelegate::CanUseWindowingControls(
    content::RenderFrameHost* requesting_frame) {
  if (!app_browser_controller_) {
    requesting_frame->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        "API called from something else than a web_app.");
    return false;
  }
  return true;
}

void BrowserWebContentsDelegate::MinimizeFromWebAPI() {
  window_->Minimize();
}

void BrowserWebContentsDelegate::MaximizeFromWebAPI() {
  window_->Maximize();
}

void BrowserWebContentsDelegate::RestoreFromWebAPI() {
  window_->Restore();
}

void BrowserWebContentsDelegate::SetResizableFromWebAPI(bool resizable) {
  // TODO(crbug.com/355026937): Eliminate this BrowserView reference.
  if (auto* browser_view = BrowserView::GetBrowserViewForBrowser(&*browser_)) {
    browser_view->SetResizableFromWebApi(resizable);
  }
}

ui::mojom::WindowShowState BrowserWebContentsDelegate::GetWindowShowState()
    const {
  return window_->GetWindowShowState();
}

bool BrowserWebContentsDelegate::CanEnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame) {
  // If the tab strip isn't editable then a drag session is in progress, and it
  // is not safe to enter fullscreen. https://crbug.com/40059349
  if (!browser_->GetTabStripModel()->delegate()->IsTabStripEditable()) {
    return false;
  }

  return exclusive_access_manager_->fullscreen_controller()
      ->CanEnterFullscreenModeForTab(requesting_frame);
}

void BrowserWebContentsDelegate::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  exclusive_access_manager_->fullscreen_controller()->EnterFullscreenModeForTab(
      requesting_frame, FullscreenTabParams{options.display_id});
}

void BrowserWebContentsDelegate::ExitFullscreenModeForTab(
    content::WebContents* web_contents) {
  exclusive_access_manager_->fullscreen_controller()->ExitFullscreenModeForTab(
      web_contents);
}

bool BrowserWebContentsDelegate::IsFullscreenForTabOrPending(
    const content::WebContents* web_contents) {
  const content::FullscreenState state = GetFullscreenState(web_contents);
  return state.target_mode == content::FullscreenMode::kContent ||
         state.target_mode == content::FullscreenMode::kPseudoContent;
}

content::FullscreenState BrowserWebContentsDelegate::GetFullscreenState(
    const content::WebContents* web_contents) const {
  return exclusive_access_manager_->fullscreen_controller()->GetFullscreenState(
      web_contents);
}

blink::mojom::DisplayMode BrowserWebContentsDelegate::GetDisplayMode(
    const content::WebContents* web_contents) {
  if (window_->IsFullscreen()) {
    return blink::mojom::DisplayMode::kFullscreen;
  }

  const BrowserWindowInterface::Type type = browser_->GetType();
  if ((type == BrowserWindowInterface::Type::TYPE_PICTURE_IN_PICTURE)) {
    return blink::mojom::DisplayMode::kPictureInPicture;
  }

  if ((type == BrowserWindowInterface::Type::TYPE_APP) ||
      (type == BrowserWindowInterface::Type::TYPE_DEVTOOLS) ||
      (type == BrowserWindowInterface::Type::TYPE_APP_POPUP)) {
    if (app_browser_controller_ &&
        app_browser_controller_->HasMinimalUiButtons()) {
      return blink::mojom::DisplayMode::kMinimalUi;
    }

    if (app_browser_controller_ &&
        app_browser_controller_->AppUsesWindowControlsOverlay() &&
        !web_contents->GetWindowsControlsOverlayRect().IsEmpty()) {
      return blink::mojom::DisplayMode::kWindowControlsOverlay;
    }

    if (app_browser_controller_ && app_browser_controller_->AppUsesTabbed()) {
      return blink::mojom::DisplayMode::kTabbed;
    }

    if (app_browser_controller_ &&
        app_browser_controller_->AppUsesUnframedMode() &&
        window_->IsUnframedModeEnabled()) {
      return blink::mojom::DisplayMode::kUnframed;
    }

    return blink::mojom::DisplayMode::kStandalone;
  }

  return blink::mojom::DisplayMode::kBrowser;
}

namespace {

// Returns the extension that owns `requesting_frame` when the frame is hosted
// in a privileged extension process, or null otherwise.
const extensions::Extension* GetExtensionForProtocolHandler(
    content::RenderFrameHost* requesting_frame) {
  content::BrowserContext* context = requesting_frame->GetBrowserContext();
  extensions::ProcessMap* process_map = extensions::ProcessMap::Get(context);
  const extensions::Extension* owner_extension =
      extensions::ProcessManager::Get(context)->GetExtensionForRenderFrameHost(
          requesting_frame);
  if (owner_extension &&
      process_map->IsPrivilegedExtensionProcess(
          *owner_extension, requesting_frame->GetProcess()->GetID())) {
    return owner_extension;
  }
  return nullptr;
}

// Creates a ProtocolHandler for a navigator.registerProtocolHandler request.
// Handlers registered from a privileged extension page are tagged with the
// owning extension's id so that they are removed when the extension is
// disabled or uninstalled.
custom_handlers::ProtocolHandler CreateProtocolHandlerForFrame(
    content::RenderFrameHost* requesting_frame,
    const std::string& protocol,
    const GURL& url) {
  if (const extensions::Extension* extension =
          GetExtensionForProtocolHandler(requesting_frame)) {
    custom_handlers::ProtocolHandler handler =
        custom_handlers::ProtocolHandler::CreateExtensionProtocolHandler(
            protocol, url, extension->id());
    // Handlers registered via the JS API go through the permission prompt and
    // so are confirmed at registration time, unlike manifest handlers.
    handler.Confirm();
    return handler;
  }
  return custom_handlers::ProtocolHandler::CreateProtocolHandler(
      protocol, url, blink::ProtocolHandlerSecurityLevel::kStrict);
}

}  // namespace

blink::ProtocolHandlerSecurityLevel
BrowserWebContentsDelegate::GetProtocolHandlerSecurityLevel(
    content::RenderFrameHost* requesting_frame) {
  return GetExtensionForProtocolHandler(requesting_frame)
             ? blink::ProtocolHandlerSecurityLevel::kExtensionFeatures
             : blink::ProtocolHandlerSecurityLevel::kStrict;
}

void BrowserWebContentsDelegate::RegisterProtocolHandler(
    content::RenderFrameHost* requesting_frame,
    const std::string& protocol,
    const GURL& url,
    bool user_gesture) {
  content::BrowserContext* context = requesting_frame->GetBrowserContext();
  if (context->IsOffTheRecord()) {
    return;
  }

  auto* web_contents =
      content::WebContents::FromRenderFrameHost(requesting_frame);

  custom_handlers::ProtocolHandler handler =
      CreateProtocolHandlerForFrame(requesting_frame, protocol, url);

  // The parameters's normalization process defined in the spec has been already
  // applied in the WebContentImpl class, so at this point it shouldn't be
  // possible to create an invalid handler.
  // https://html.spec.whatwg.org/multipage/system-state.html#normalize-protocol-handler-parameters
  DCHECK(handler.IsValid());

  custom_handlers::ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(context);
  if (registry->SilentlyHandleRegisterHandlerRequest(handler)) {
    return;
  }

  // TODO(carlscab): This should probably be FromFrame() once it becomes
  // PageSpecificContentSettingsDelegate
  auto* page_content_settings_delegate =
      PageSpecificContentSettingsDelegate::FromWebContents(web_contents);
  if (!user_gesture) {
    page_content_settings_delegate->set_pending_protocol_handler(handler);
    page_content_settings_delegate->set_previous_protocol_handler(
        registry->GetHandlerFor(handler.protocol()));
    window_->GetLocationBar()->UpdateContentSettingsIcons();
    return;
  }

  // Make sure content-setting icon is turned off in case the page does
  // ungestured and gestured RPH calls.
  page_content_settings_delegate->ClearPendingProtocolHandler();
  window_->GetLocationBar()->UpdateContentSettingsIcons();

  if (registry->registration_mode() ==
      custom_handlers::RphRegistrationMode::kAutoAccept) {
    registry->OnAcceptRegisterProtocolHandler(handler);
    return;
  }

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  if (permission_request_manager) {
    auto blocker = web_contents->ForSecurityDropFullscreen(
        /*display_id=*/display::kInvalidDisplayId);
    if (!blocker) {
      return;
    }

    permission_request_manager->AddRequest(
        requesting_frame,
        std::make_unique<
            custom_handlers::RegisterProtocolHandlerPermissionRequest>(
            registry, handler, url, std::move(*blocker)));
  }
}

void BrowserWebContentsDelegate::UnregisterProtocolHandler(
    content::RenderFrameHost* requesting_frame,
    const std::string& protocol,
    const GURL& url,
    bool user_gesture) {
  // user_gesture will be used in case we decide to have confirmation bubble
  // for user while un-registering the handler.
  content::BrowserContext* context = requesting_frame->GetBrowserContext();
  if (context->IsOffTheRecord()) {
    return;
  }

  custom_handlers::ProtocolHandler handler =
      CreateProtocolHandlerForFrame(requesting_frame, protocol, url);

  custom_handlers::ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(context);
  registry->RemoveHandler(handler);
}

void BrowserWebContentsDelegate::FindReply(content::WebContents* web_contents,
                                           int request_id,
                                           int number_of_matches,
                                           const gfx::Rect& selection_rect,
                                           int active_match_ordinal,
                                           bool final_update) {
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents);
  if (!find_tab_helper) {
    return;
  }

  find_tab_helper->HandleFindReply(request_id, number_of_matches,
                                   selection_rect, active_match_ordinal,
                                   final_update);
}

void BrowserWebContentsDelegate::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  const extensions::Extension* extension =
      GetExtensionForOrigin(browser_->GetProfile(), request.security_origin);
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), extension);
}

void BrowserWebContentsDelegate::ProcessSelectAudioOutput(
    const content::SelectAudioOutputRequest& request,
    content::SelectAudioOutputCallback callback) {
#if defined(TOOLKIT_VIEWS)
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessSelectAudioOutputRequest(
      &*browser_, request, std::move(callback));
#else
  std::move(callback).Run(
      base::unexpected(content::SelectAudioOutputError::kUnknown));
#endif
}

bool BrowserWebContentsDelegate::CheckMediaAccessPermission(
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

std::string BrowserWebContentsDelegate::GetTitleForMediaControls(
    content::WebContents* web_contents) {
  return app_browser_controller_
             ? app_browser_controller_->GetTitleForMediaControls()
             : std::string();
}

void BrowserWebContentsDelegate::GetAIPageContent(
    content::WebContents* web_contents,
    bool include_actionable_elements,
    base::OnceCallback<void(const std::string&)> callback) {
  auto options = include_actionable_elements
                     ? optimization_guide::ActionableAIPageContentOptions(
                           /*on_critical_path=*/false)
                     : optimization_guide::DefaultAIPageContentOptions(
                           /*on_critical_path=*/false);

  optimization_guide::GetAIPageContent(
      web_contents, std::move(options),
      base::BindOnce([](optimization_guide::AIPageContentResultOrError result)
                         -> std::string {
        if (!result.has_value()) {
          return "";
        }
        return result->proto.SerializeAsString();
      }).Then(std::move(callback)));
}

void BrowserWebContentsDelegate::PrintCrossProcessSubframe(
    content::WebContents* web_contents,
    const gfx::Rect& rect,
    int document_cookie,
    content::RenderFrameHost* subframe_host) const {
  auto* client = printing::PrintCompositeClient::FromWebContents(web_contents);
  if (client) {
    client->PrintCrossProcessSubframe(rect, document_cookie, subframe_host);
  }
}

void BrowserWebContentsDelegate::CapturePaintPreviewOfSubframe(
    content::WebContents* web_contents,
    const gfx::Rect& rect,
    const base::UnguessableToken& guid,
    content::RenderFrameHost* render_frame_host) {
  auto* client =
      paint_preview::PaintPreviewClient::FromWebContents(web_contents);
  if (client) {
    client->CaptureSubframePaintPreview(guid, rect, render_frame_host);
  }
}
