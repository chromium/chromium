// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/web_view/ash_web_view_impl.h"

#include "ash/public/cpp/window_properties.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {
void FixZoomLevelToOne(content::RenderFrameHost* render_frame_host) {
  content::HostZoomMap* zoom_map =
      content::HostZoomMap::Get(render_frame_host->GetSiteInstance());
  zoom_map->SetTemporaryZoomLevel(render_frame_host->GetGlobalId(), 1.0);
}
}  // namespace

AshWebViewImpl::AshWebViewImpl(const InitParams& params) : params_(params) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  InitWebContents(profile);
  InitLayout(profile);
}

AshWebViewImpl::~AshWebViewImpl() {
  Observe(nullptr);
  web_contents_->SetDelegate(nullptr);
}

gfx::NativeView AshWebViewImpl::GetNativeView() {
  return web_contents_->GetNativeView();
}

void AshWebViewImpl::ChildPreferredSizeChanged(views::View* child) {
  DCHECK_EQ(web_view_, child);
  SetPreferredSize(web_view_->GetPreferredSize());
}

void AshWebViewImpl::Layout(PassKey) {
  web_view_->SetBoundsRect(GetContentsBounds());
}

void AshWebViewImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AshWebViewImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool AshWebViewImpl::GoBack() {
  if (web_contents_->GetController().CanGoBack()) {
    web_contents_->GetController().GoBack();
    return true;
  }
  return false;
}

void AshWebViewImpl::Navigate(const GURL& url) {
  content::NavigationController::LoadURLParams params(url);
  web_contents_->GetController().LoadURLWithParams(params);
}

const GURL& AshWebViewImpl::GetVisibleURL() {
  return web_contents_->GetVisibleURL();
}

bool AshWebViewImpl::IsErrorDocument() {
  return web_contents_->GetPrimaryMainFrame()->IsErrorDocument();
}

views::View* AshWebViewImpl::GetInitiallyFocusedView() {
  return web_view_;
}

void AshWebViewImpl::SetCornerRadii(const gfx::RoundedCornersF& corner_radii) {
  web_view_->holder()->SetCornerRadii(corner_radii);
}

const base::UnguessableToken& AshWebViewImpl::GetMediaSessionRequestId() {
  return content::MediaSession::GetRequestIdFromWebContents(
      web_contents_.get());
}

void AshWebViewImpl::AddedToWidget() {
  UpdateMinimizeOnBackProperty();
  AshWebView::AddedToWidget();

  // Apply rounded corners. This can't be done earlier since it
  // requires `web_view_->holder()->native_view()` to be initialized.
  if (params_.rounded_corners.has_value()) {
    web_view_->holder()->SetCornerRadii(params_.rounded_corners.value());
  }
}

bool AshWebViewImpl::IsWebContentsCreationOverridden(
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  if (params_.suppress_navigation) {
    NotifyDidSuppressNavigation(target_url,
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                /*from_user_gesture=*/true);
    return true;
  }
  return content::WebContentsDelegate::IsWebContentsCreationOverridden(
      source_site_instance, window_container_type, opener_url, frame_name,
      target_url);
}

content::WebContents* AshWebViewImpl::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  if (params_.suppress_navigation) {
    NotifyDidSuppressNavigation(params.url, params.disposition,
                                params.user_gesture);
    return nullptr;
  }
  return content::WebContentsDelegate::OpenURLFromTab(
      source, params, std::move(navigation_handle_callback));
}

void AshWebViewImpl::ResizeDueToAutoResize(content::WebContents* web_contents,
                                           const gfx::Size& new_size) {
  DCHECK_EQ(web_contents_.get(), web_contents);
  web_view_->SetPreferredSize(new_size);
}

bool AshWebViewImpl::TakeFocus(content::WebContents* web_contents,
                               bool reverse) {
  DCHECK_EQ(web_contents_.get(), web_contents);
  auto* focus_manager = GetFocusManager();
  if (focus_manager) {
    focus_manager->ClearNativeFocus();
  }
  return false;
}

void AshWebViewImpl::NavigationStateChanged(
    content::WebContents* web_contents,
    content::InvalidateTypes changed_flags) {
  DCHECK_EQ(web_contents_.get(), web_contents);
  UpdateCanGoBack();
}

void AshWebViewImpl::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  if (!params_.can_record_media) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED,
        std::unique_ptr<content::MediaStreamUI>());
    return;
  }
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), /*extension=*/nullptr);
}

bool AshWebViewImpl::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  if (!params_.can_record_media) {
    return false;
  }
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(render_frame_host, security_origin, type);
}

std::string AshWebViewImpl::GetTitleForMediaControls(
    content::WebContents* web_contents) {
  if (!params_.source_title.empty()) {
    return params_.source_title;
  }

  return content::WebContentsDelegate::GetTitleForMediaControls(web_contents);
}

void AshWebViewImpl::DidStopLoading() {
  for (auto& observer : observers_) {
    observer.DidStopLoading();
  }
}

void AshWebViewImpl::OnFocusChangedInPage(
    content::FocusedNodeDetails* details) {
  // When navigating to the |web_contents_|, it may not focus it. Request focus
  // as needed. This is a workaround to get a non-empty rect of the focused
  // node. See details in b/177047240.
  auto* native_view = web_contents_->GetContentNativeView();
  if (native_view && !native_view->HasFocus()) {
    web_contents_->Focus();
  }

  for (auto& observer : observers_) {
    observer.DidChangeFocusedNode(details->node_bounds_in_screen);
  }
}

void AshWebViewImpl::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  if (new_host != new_host->GetOutermostMainFrame()) {
    return;
  }
  if (params_.fix_zoom_level_to_one) {
    FixZoomLevelToOne(new_host);
  }
}

void AshWebViewImpl::PrimaryPageChanged(content::Page& page) {
  DCHECK_EQ(&page.GetMainDocument(), web_contents_->GetPrimaryMainFrame());
  if (!web_contents_->GetRenderWidgetHostView()) {
    return;
  }

  if (!params_.enable_auto_resize) {
    return;
  }

  gfx::Size min_size(1, 1);
  if (params_.min_size) {
    min_size.SetToMax(params_.min_size.value());
  }

  gfx::Size max_size(INT_MAX, INT_MAX);
  if (params_.max_size) {
    max_size.SetToMin(params_.max_size.value());
  }

  web_contents_->GetRenderWidgetHostView()->EnableAutoResize(min_size,
                                                             max_size);
}

void AshWebViewImpl::NavigationEntriesDeleted() {
  UpdateCanGoBack();
}

void AshWebViewImpl::InitWebContents(Profile* profile) {
  auto web_contents_params = content::WebContents::CreateParams(
      profile, content::SiteInstance::Create(profile));
  web_contents_params.enable_wake_locks = params_.enable_wake_locks;
  web_contents_ = content::WebContents::Create(web_contents_params);

  web_contents_->SetDelegate(this);
  Observe(web_contents_.get());

  // Use a transparent background.
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      web_contents_.get(), SK_ColorTRANSPARENT);

  // If requested, suppress navigation.
  if (params_.suppress_navigation) {
    web_contents_->GetMutableRendererPrefs()
        ->browser_handles_all_top_level_requests = true;
    web_contents_->SyncRendererPrefs();
  }

  if (params_.fix_zoom_level_to_one) {
    FixZoomLevelToOne(web_contents_->GetPrimaryMainFrame());
  }
}

void AshWebViewImpl::InitLayout(Profile* profile) {
  web_view_ = AddChildView(std::make_unique<views::WebView>(profile));
  web_view_->SetID(ash::kAshWebViewChildWebViewId);
  web_view_->SetWebContents(web_contents_.get());
}

void AshWebViewImpl::NotifyDidSuppressNavigation(
    const GURL& url,
    WindowOpenDisposition disposition,
    bool from_user_gesture) {
  // Note that we post notification to |observers_| as an observer may cause
  // |this| to be deleted during handling of the event which is unsafe to do
  // until the original navigation sequence has been completed.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const base::WeakPtr<AshWebViewImpl>& self, GURL url,
             WindowOpenDisposition disposition, bool from_user_gesture) {
            if (self) {
              for (auto& observer : self->observers_) {
                observer.DidSuppressNavigation(url, disposition,
                                               from_user_gesture);

                // We need to check |self| to confirm that |observer| did not
                // delete |this|. If |this| is deleted, we quit.
                if (!self) {
                  return;
                }
              }
            }
          },
          weak_factory_.GetWeakPtr(), url, disposition, from_user_gesture));
}

void AshWebViewImpl::UpdateCanGoBack() {
  const bool can_go_back = web_contents_->GetController().CanGoBack();
  if (can_go_back_ == can_go_back) {
    return;
  }

  can_go_back_ = can_go_back;

  UpdateMinimizeOnBackProperty();

  for (auto& observer : observers_) {
    observer.DidChangeCanGoBack(can_go_back_);
  }
}

void AshWebViewImpl::UpdateMinimizeOnBackProperty() {
  const bool minimize_on_back = params_.minimize_on_back_key && !can_go_back_;
  views::Widget* widget = GetWidget();
  if (widget) {
    widget->GetNativeWindow()->SetProperty(ash::kMinimizeOnBackKey,
                                           minimize_on_back);
  }
}

BEGIN_METADATA(AshWebViewImpl)
END_METADATA
