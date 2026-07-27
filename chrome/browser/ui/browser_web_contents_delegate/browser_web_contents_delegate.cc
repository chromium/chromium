// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_web_contents_delegate/browser_web_contents_delegate.h"

#include "base/metrics/histogram_macros.h"
#include "base/notimplemented.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/actor/actor_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/blocked_content/chrome_popup_navigation_delegate.h"
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/keyboard_lock_controller.h"
#include "chrome/browser/ui/exclusive_access/pointer_lock_controller.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/status_bubble_views.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "components/blocked_content/list_item_position.h"
#include "components/blocked_content/popup_blocker.h"
#include "components/blocked_content/popup_tracker.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/tabs/public/split_tab_data.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/common/drop_data.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "ui/base/base_window.h"

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

BrowserWebContentsDelegate::BrowserWebContentsDelegate(
    BrowserWindowInterface* browser,
    ExclusiveAccessManager& exclusive_access_manager,
    chrome::BrowserCommandController& command_controller,
    UnloadController& unload_controller,
    web_app::AppBrowserController* app_browser_controller,
    BrowserWindow& window,
    DesktopBrowserWindowCapabilities& capabilities)
    : exclusive_access_manager_(exclusive_access_manager),
      command_controller_(command_controller),
      unload_controller_(unload_controller),
      app_browser_controller_(app_browser_controller),
      window_(window),
      capabilities_(capabilities),
      browser_(*browser),
      scoped_data_holder_(browser->GetUnownedUserDataHost(), *this) {}

BrowserWebContentsDelegate* BrowserWebContentsDelegate::From(
    BrowserWindowInterface* browser) {
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
    browser_->GetBrowserForMigrationOnly()->ScheduleUIUpdate(source,
                                                             changed_flags);
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
  browser_->GetBrowserForMigrationOnly()->ScheduleUIUpdate(
      source, content::INVALIDATE_TYPE_LOAD);
  browser_->GetBrowserForMigrationOnly()->UpdateWindowForLoadingStateChanged(
      source, should_show_loading_ui);
}

void BrowserWebContentsDelegate::CloseContents(content::WebContents* source) {
  if (unload_controller_->CanCloseContents(source)) {
    chrome::CloseWebContents(browser_->GetBrowserForMigrationOnly(), source,
                             true);
  }
}
