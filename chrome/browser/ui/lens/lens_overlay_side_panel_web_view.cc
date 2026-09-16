// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_side_panel_web_view.h"

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_event_handler.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/tabs/public/tab_interface.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/window_open_disposition.h"

using SidePanelWebUIViewT_LensSidePanelUntrustedUI =
    SidePanelWebUIViewT<lens::LensSidePanelUntrustedUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT_LensSidePanelUntrustedUI,
                        SidePanelWebUIViewT)
END_METADATA

namespace {}  // namespace

LensOverlaySidePanelWebView::LensOverlaySidePanelWebView(
    content::BrowserContext* browser_context,
    lens::LensOverlaySidePanelCoordinator* coordinator,
    SidePanelEntryScope& scope)
    : SidePanelWebUIViewT(
          scope,
          base::RepeatingClosure(),
          base::RepeatingClosure(),
          std::make_unique<
              WebUIContentsWrapperT<lens::LensSidePanelUntrustedUI>>(
              GURL(chrome::kChromeUILensUntrustedSidePanelAPIURL),
              Profile::FromBrowserContext(browser_context),
              /*task_manager_string_id=*/IDS_SIDE_PANEL_COMPANION_TITLE,
              /*esc_closes_ui=*/false)),
      coordinator_(coordinator) {
  CHECK(coordinator);
  // Register the modal dialog manager for this side panel web contents so
  // browser dialogs can open when requested by the side panel WebUI.
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      GetWebContents());
  web_modal::WebContentsModalDialogManager::FromWebContents(GetWebContents())
      ->SetDelegate(coordinator);
}

LensOverlaySidePanelWebView::~LensOverlaySidePanelWebView() {
  if (coordinator_) {
    coordinator_->WebViewClosing();
    ClearCoordinator();
  }
}

void LensOverlaySidePanelWebView::ClearCoordinator() {
  web_modal::WebContentsModalDialogManager::FromWebContents(GetWebContents())
      ->SetDelegate(nullptr);
  coordinator_ = nullptr;
}

bool LensOverlaySidePanelWebView::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  return false;
}

content::WebContents* LensOverlaySidePanelWebView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  // Note that `navigation_handle_callback` is dropped without being run on all
  // of the early returns below. Callers must tolerate the callback never
  // running, which is the same contract as any other rejected navigation.
  if (!coordinator_ || !coordinator_->GetLensSearchController()) {
    return nullptr;
  }
  tabs::TabInterface* tab =
      coordinator_->GetLensSearchController()->GetTabInterface();
  // A tab that is not attached to a browser window has nowhere to open the
  // URL, so drop the request instead of navigating.
  if (!tab || !tab->GetBrowserWindowInterface()) {
    return nullptr;
  }

  // Resolve the frame that requested this navigation.
  // `initiator_frame_token` is preferred because it identifies the frame even
  // if it has since navigated; the routing ID is only a fallback for requests
  // that do not carry a token.
  content::RenderFrameHost* source_rfh = nullptr;
  if (params.initiator_frame_token.has_value()) {
    source_rfh = content::RenderFrameHost::FromFrameToken(
        content::GlobalRenderFrameHostToken(
            params.initiator_process_id, params.initiator_frame_token.value()));
  } else {
    source_rfh = content::RenderFrameHost::FromID(
        params.source_render_process_id, params.source_render_frame_id);
  }
  // Only an initiator that actually lives in this side panel may be trusted;
  // treat anything else as an unknown initiator so that its state (notably
  // user activation) is not attributed to this side panel.
  if (source_rfh && content::WebContents::FromRenderFrameHost(source_rfh) !=
                        GetWebContents()) {
    source_rfh = nullptr;
  }

  // Everything hosted by this WebContents is renderer content that the browser
  // must not trust, including the chrome-untrusted:// WebUI main frame itself,
  // so the scheme check below applies to every initiator without exception.
  //
  // The side panel only renders links from web content, so restrict forwarded
  // navigations to web schemes. This intentionally also drops external
  // protocol links (e.g. mailto:).
  if (!params.url.SchemeIsHTTPOrHTTPS()) {
    return nullptr;
  }

  content::OpenURLParams modified_params = params;

  // The two checks below exist to catch a renderer lying about how a navigation
  // was requested. They do not apply to requests the browser itself created,
  // such as a context menu command, because those values are not renderer
  // claims in the first place.
  //
  // A renderer hosted here cannot forge this bit. Navigator::RequestOpenURL()
  // hardcodes `is_renderer_initiated` to true, and only clears it for a WebUI
  // whose TrustPolicy is kTrusted. The side panel is LensSidePanelUntrustedUI,
  // an UntrustedWebUIController, which is kUntrusted, and the results frame is
  // ordinary web content with no WebUI at all.
  if (params.is_renderer_initiated) {
    // Check that user_gesture is really true, by confirming that there was a
    // recent activation in the source render frame host. This is best effort:
    // an unknown initiator is treated as having no activation.
    if (modified_params.user_gesture &&
        (!source_rfh || !source_rfh->HasTransientUserActivation())) {
      modified_params.user_gesture = false;
    }

    // Only forward dispositions that open a new container and that
    // `blocked_content::ConsiderForPopupBlocking()` actually evaluates.
    // Anything else is demoted to NEW_FOREGROUND_TAB so that the popup blocker
    // still runs and the underlying tab is never navigated. Notably,
    // SAVE_TO_DISK and OFF_THE_RECORD are deliberately excluded: the popup
    // blocker ignores both, so allowing them would let the results frame start
    // downloads or open off-the-record windows with no user gesture.
    //
    // `default` is used deliberately so that any disposition added in the
    // future is demoted rather than silently forwarded.
    switch (modified_params.disposition) {
      case WindowOpenDisposition::NEW_FOREGROUND_TAB:
      case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      case WindowOpenDisposition::NEW_POPUP:
      case WindowOpenDisposition::NEW_WINDOW:
        break;
      default:
        modified_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
        break;
    }
  }

  // Reset frame_tree_node_id to prevent mismatched frame tree lookups in the
  // target navigation controller.
  modified_params.frame_tree_node_id = content::FrameTreeNodeId();

  // Pass the main tab's WebContents as the source so the navigation pipeline
  // has the correct context to evaluate disposition and blocking rules.
  content::WebContents* tab_contents = tab->GetContents();
  if (tab_contents && tab_contents->GetDelegate()) {
    tab_contents->GetDelegate()->OpenURLFromTab(
        tab_contents, modified_params, std::move(navigation_handle_callback));
  }

  // Always return nullptr, even when the navigation succeeded and created a
  // WebContents. Returning it would make WebContentsImpl::OpenURL() notify
  // WebContentsObserver::DidOpenRequestedURL(), and
  // LensOverlaySidePanelCoordinator observes this WebContents and reacts to
  // that notification by opening the URL in the browser itself. The URL would
  // then be opened twice for a single request. That observer exists to service
  // the WebContentsImpl::CreateNewWindow() path (e.g. target="_blank"), which
  // this method does not go through.
  return nullptr;
}

bool LensOverlaySidePanelWebView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (!coordinator_) {
    return false;
  }
  return coordinator_->GetLensSearchController()
      ->lens_overlay_event_handler()
      ->HandleKeyboardEvent(source, event, GetFocusManager());
}

void LensOverlaySidePanelWebView::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  // Note: This is needed for taking screenshots via the feedback form on the
  // side panel.
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), /*extension=*/nullptr);
}

BEGIN_METADATA(LensOverlaySidePanelWebView)
END_METADATA
