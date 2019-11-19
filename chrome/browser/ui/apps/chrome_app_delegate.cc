// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/chrome_app_delegate.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/platform_apps/audio_focus_web_contents_observer.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color_chooser.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/web_contents_sizer.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/mojom/app_window.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/buildflags/buildflags.h"
#include "services/service_manager/public/cpp/interface_provider.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/printing_init.h"
#endif

namespace {

// Time to wait for an app window to show before allowing Chrome to quit.
int kAppWindowFirstShowTimeoutSeconds = 10;

bool disable_external_open_for_testing_ = false;

// Opens a URL with Chromium (not external browser) with the right profile.
content::WebContents* OpenURLFromTabInternal(
    content::BrowserContext* context,
    const content::OpenURLParams& params) {
  NavigateParams new_tab_params(static_cast<Browser*>(nullptr), params.url,
                                params.transition);
  new_tab_params.FillNavigateParamsFromOpenURLParams(params);

  // Force all links to open in a new tab, even if they were trying to open a
  // window.
  if (params.disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    new_tab_params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  } else {
    new_tab_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    new_tab_params.window_action = NavigateParams::SHOW_WINDOW;
  }

  new_tab_params.initiating_profile = Profile::FromBrowserContext(context);
  Navigate(&new_tab_params);

  return new_tab_params.navigated_or_inserted_contents;
}

void OpenURLAfterCheckIsDefaultBrowser(
    std::unique_ptr<content::WebContents> source,
    const content::OpenURLParams& params,
    shell_integration::DefaultWebClientState state) {
  // Open a URL based on if this browser instance is the default system browser.
  // If it is the default, open the URL directly instead of asking the system to
  // open it.
  Profile* profile = Profile::FromBrowserContext(source->GetBrowserContext());
  DCHECK(profile);
  if (!profile)
    return;
  switch (state) {
    case shell_integration::IS_DEFAULT:
      OpenURLFromTabInternal(profile, params);
      return;
    case shell_integration::NOT_DEFAULT:
    case shell_integration::UNKNOWN_DEFAULT:
    case shell_integration::OTHER_MODE_IS_DEFAULT:
      platform_util::OpenExternal(profile, params.url);
      return;
    case shell_integration::NUM_DEFAULT_STATES:
      break;
  }
  NOTREACHED();
}

}  // namespace

// static
void ChromeAppDelegate::RelinquishKeepAliveAfterTimeout(
    const base::WeakPtr<ChromeAppDelegate>& chrome_app_delegate) {
  // Resetting the ScopedKeepAlive may cause nested destruction of the
  // ChromeAppDelegate which also resets the ScopedKeepAlive. To avoid this,
  // move the ScopedKeepAlive out to here and let it fall out of scope.
  if (chrome_app_delegate.get() && chrome_app_delegate->is_hidden_)
    std::unique_ptr<ScopedKeepAlive>(
        std::move(chrome_app_delegate->keep_alive_));
}

class ChromeAppDelegate::NewWindowContentsDelegate
    : public content::WebContentsDelegate {
 public:
  NewWindowContentsDelegate() {}
  ~NewWindowContentsDelegate() override {}

  void BecomeOwningDeletageOf(
      std::unique_ptr<content::WebContents> web_contents) {
    web_contents->SetDelegate(this);
    owned_contents_.push_back(std::move(web_contents));
  }

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;

 private:
  std::vector<std::unique_ptr<content::WebContents>> owned_contents_;

  DISALLOW_COPY_AND_ASSIGN(NewWindowContentsDelegate);
};

content::WebContents*
ChromeAppDelegate::NewWindowContentsDelegate::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  if (source) {
    // This NewWindowContentsDelegate was given ownership of the incoming
    // WebContents by being assigned as its delegate within
    // ChromeAppDelegate::AddNewContents(), but this is the first time
    // NewWindowContentsDelegate actually sees the WebContents. Here ownership
    // is captured and passed to OpenURLAfterCheckIsDefaultBrowser(), which
    // destroys it after the default browser worker completes.
    std::unique_ptr<content::WebContents> owned_source;
    for (auto it = owned_contents_.begin(); it != owned_contents_.end(); ++it) {
      if (it->get() == source) {
        owned_source = std::move(*it);
        owned_contents_.erase(it);
        break;
      }
    }
    DCHECK(owned_source);

    // Object lifetime notes: StartCheckIsDefault() takes lifetime ownership of
    // check_if_default_browser_worker and will clean up after the asynchronous
    // tasks.
    scoped_refptr<shell_integration::DefaultBrowserWorker>
        check_if_default_browser_worker =
            new shell_integration::DefaultBrowserWorker(
                base::Bind(&OpenURLAfterCheckIsDefaultBrowser,
                           base::Passed(&owned_source), params));
    check_if_default_browser_worker->StartCheckIsDefault();
  }
  return NULL;
}

ChromeAppDelegate::ChromeAppDelegate(bool keep_alive)
    : has_been_shown_(false),
      is_hidden_(true),
      for_lock_screen_app_(false),
      new_window_contents_delegate_(new NewWindowContentsDelegate()) {
  if (keep_alive) {
    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::CHROME_APP_DELEGATE, KeepAliveRestartOption::DISABLED);
  }
  registrar_.Add(this,
                 chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
}

ChromeAppDelegate::~ChromeAppDelegate() {
  // Unregister now to prevent getting notified if |keep_alive_| is the last.
  terminating_callback_.Reset();
}

void ChromeAppDelegate::DisableExternalOpenForTesting() {
  disable_external_open_for_testing_ = true;
}

void ChromeAppDelegate::InitWebContents(content::WebContents* web_contents) {
  favicon::CreateContentFaviconDriverForWebContents(web_contents);

#if BUILDFLAG(ENABLE_PRINTING)
  printing::InitializePrinting(web_contents);
#endif
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents);

  apps::AudioFocusWebContentsObserver::CreateForWebContents(web_contents);

  zoom::ZoomController::CreateForWebContents(web_contents);
}

void ChromeAppDelegate::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  if (!chrome::IsRunningInForcedAppMode()) {
    // Due to a bug in the way apps reacted to default zoom changes, some apps
    // can incorrectly have host level zoom settings. These aren't wanted as
    // apps cannot be zoomed, so are removed. This should be removed if apps
    // can be made to zoom again.
    // See http://crbug.com/446759 for more details.
    content::WebContents* web_contents =
        content::WebContents::FromRenderViewHost(render_view_host);
    DCHECK(web_contents);
    content::HostZoomMap* zoom_map =
        content::HostZoomMap::GetForWebContents(web_contents);
    DCHECK(zoom_map);
    zoom_map->SetZoomLevelForHost(web_contents->GetURL().host(), 0);
  }
}

void ChromeAppDelegate::ResizeWebContents(content::WebContents* web_contents,
                                          const gfx::Size& size) {
  ::ResizeWebContents(web_contents, gfx::Rect(size));
}

content::WebContents* ChromeAppDelegate::OpenURLFromTab(
    content::BrowserContext* context,
    content::WebContents* source,
    const content::OpenURLParams& params) {
  return OpenURLFromTabInternal(context, params);
}

void ChromeAppDelegate::AddNewContents(
    content::BrowserContext* context,
    std::unique_ptr<content::WebContents> new_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture) {
  if (!disable_external_open_for_testing_) {
    // We don't really want to open a window for |new_contents|, but we need to
    // capture its intended navigation. Here we give ownership to the
    // NewWindowContentsDelegate, which will dispose of the contents once
    // a navigation is captured.
    new_window_contents_delegate_->BecomeOwningDeletageOf(
        std::move(new_contents));
    return;
  }

  chrome::ScopedTabbedBrowserDisplayer displayer(
      Profile::FromBrowserContext(context));
  // Force all links to open in a new tab, even if they were trying to open a
  // new window.
  disposition = disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB
                    ? disposition
                    : WindowOpenDisposition::NEW_FOREGROUND_TAB;
  chrome::AddWebContents(displayer.browser(), NULL, std::move(new_contents),
                         disposition, initial_rect);
}

content::ColorChooser* ChromeAppDelegate::ShowColorChooser(
    content::WebContents* web_contents,
    SkColor initial_color) {
  return chrome::ShowColorChooser(web_contents, initial_color);
}

void ChromeAppDelegate::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}

void ChromeAppDelegate::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), extension);
}

bool ChromeAppDelegate::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(render_frame_host, security_origin, type,
                                   extension);
}

int ChromeAppDelegate::PreferredIconSize() const {
#if defined(OS_CHROMEOS)
  // Use a size appropriate for the ash shelf (see ash::kShelfSize).
  return extension_misc::EXTENSION_ICON_MEDIUM;
#else
  return extension_misc::EXTENSION_ICON_SMALL;
#endif
}

void ChromeAppDelegate::SetWebContentsBlocked(
    content::WebContents* web_contents,
    bool blocked) {
  if (!blocked)
    web_contents->Focus();
  // RenderViewHost may be NULL during shutdown.
  content::RenderFrameHost* host = web_contents->GetMainFrame();
  if (host) {
    mojo::Remote<extensions::mojom::AppWindow> app_window;
    host->GetRemoteInterfaces()->GetInterface(
        app_window.BindNewPipeAndPassReceiver());
    app_window->SetVisuallyDeemphasized(blocked);
  }
}

bool ChromeAppDelegate::IsWebContentsVisible(
    content::WebContents* web_contents) {
  return platform_util::IsVisible(web_contents->GetNativeView());
}

void ChromeAppDelegate::SetTerminatingCallback(const base::Closure& callback) {
  terminating_callback_ = callback;
}

void ChromeAppDelegate::OnHide() {
  is_hidden_ = true;
  if (has_been_shown_) {
    keep_alive_.reset();
    return;
  }

  // Hold on to the keep alive for some time to give the app a chance to show
  // the window.
  base::PostDelayedTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&ChromeAppDelegate::RelinquishKeepAliveAfterTimeout,
                     weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kAppWindowFirstShowTimeoutSeconds));
}

void ChromeAppDelegate::OnShow() {
  has_been_shown_ = true;
  is_hidden_ = false;
  keep_alive_ = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::CHROME_APP_DELEGATE, KeepAliveRestartOption::DISABLED);
}

bool ChromeAppDelegate::TakeFocus(content::WebContents* web_contents,
                                  bool reverse) {
  if (!for_lock_screen_app_)
    return false;
#if defined(OS_CHROMEOS)
  return lock_screen_apps::StateController::Get()->HandleTakeFocus(web_contents,
                                                                   reverse);
#else
  return false;
#endif
}

content::PictureInPictureResult ChromeAppDelegate::EnterPictureInPicture(
    content::WebContents* web_contents,
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size) {
  return PictureInPictureWindowManager::GetInstance()->EnterPictureInPicture(
      web_contents, surface_id, natural_size);
}

void ChromeAppDelegate::ExitPictureInPicture() {
  PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
}

void ChromeAppDelegate::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);
  if (!terminating_callback_.is_null())
    terminating_callback_.Run();
}
