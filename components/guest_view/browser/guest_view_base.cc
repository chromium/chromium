// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/guest_view_base.h"

#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "components/guest_view/browser/guest_view_event.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

using content::WebContents;

namespace guest_view {

namespace {

const void* const kGuestPageDelegateOwnershipUserDataKey =
    &kGuestPageDelegateOwnershipUserDataKey;

void DestroyGuestIfUnattached(GuestViewBase* guest) {
  std::unique_ptr<GuestViewBase> owned_guest =
      guest->GetGuestViewManager()->TransferOwnership(guest);
  owned_guest.reset();
}

}  // namespace

SetSizeParams::SetSizeParams() = default;
SetSizeParams::~SetSizeParams() = default;

// This observer ensures that unattached guests don't outlive their owner
// WebContents. It also tracks when the embedder's fullscreen is toggled so the
// guest can change itself accordingly.
// This is not used when `kGuestViewMPArch` is enabled.
class GuestViewBase::OwnerContentsObserver : public WebContentsObserver {
 public:
  explicit OwnerContentsObserver(base::SafeRef<GuestViewBase> guest,
                                 WebContents* owner_web_contents)
      : WebContentsObserver(owner_web_contents), guest_(guest) {
    // This is only used by the inner WebContents implementation. The MPArch
    // version has the GuestViewBase observe the owner directly.
    CHECK(!base::FeatureList::IsEnabled(features::kGuestViewMPArch));
  }

  OwnerContentsObserver(const OwnerContentsObserver&) = delete;
  OwnerContentsObserver& operator=(const OwnerContentsObserver&) = delete;

  ~OwnerContentsObserver() override = default;

  // WebContentsObserver implementation.
  void WebContentsDestroyed() override {
    // Once attached, the guest can't outlive its owner WebContents.
    DCHECK_EQ(guest_->element_instance_id(), kInstanceIDNone);

    // Defensively clear the guest's `owner_rfh_id_`, since a unique_ptr
    // to the guest may be passed through asynchronous calls early in its
    // initialization, and it's possible that the owner WebContents could be
    // destroyed during this process. Lookups using an id would still be safe,
    // but we clear this anyway to avoid unexpected lookups during destruction.
    guest_->owner_rfh_id_ = content::GlobalRenderFrameHostId();
    DestroyGuestIfUnattached(&*guest_);
  }

  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override {
    if (!IsGuestInitialized()) {
      return;
    }

    is_fullscreen_ = entered_fullscreen;
    guest_->EmbedderFullscreenToggled(is_fullscreen_);
  }

  void PrimaryMainFrameWasResized(bool width_changed) override {
    if (!IsGuestInitialized()) {
      return;
    }

    bool current_fullscreen = web_contents()->IsFullscreen();
    if (is_fullscreen_ && !current_fullscreen) {
      is_fullscreen_ = false;
      guest_->EmbedderFullscreenToggled(is_fullscreen_);
    }
  }

  void DidUpdateAudioMutingState(bool muted) override {
    if (!IsGuestInitialized()) {
      return;
    }
    guest_->OnOwnerAudioMutedStateUpdated(muted);
  }

 private:
  bool IsGuestInitialized() { return guest_->web_contents(); }

  bool is_fullscreen_ = false;
  const base::SafeRef<GuestViewBase> guest_;
};

// This observer ensures that a GuestViewBase is destroyed if if its opener
// WebContents goes away before the GuestViewBase is attached.
// TODO(mcnee): This behaviour is WebViewGuest specific and could be moved there
// instead.
class GuestViewBase::OpenerLifetimeObserver : public WebContentsObserver {
 public:
  explicit OpenerLifetimeObserver(GuestViewBase* guest)
      : WebContentsObserver(guest->GetOpener()->web_contents()),
        guest_(guest) {}

  OpenerLifetimeObserver(const OpenerLifetimeObserver&) = delete;
  OpenerLifetimeObserver& operator=(const OpenerLifetimeObserver&) = delete;

  ~OpenerLifetimeObserver() override = default;

  // WebContentsObserver implementation.
  void WebContentsDestroyed() override {
    // If the opener is destroyed and the guest has not been attached, then
    // destroy the guest.
    // Note that the guest contents may be owned by content/ at this point. In
    // this case, we expect content/ to safely destroy the contents without
    // accessing delegate methods of the destroyed guest.
    // Destroys `this`.
    DestroyGuestIfUnattached(guest_);
  }

 private:
  raw_ptr<GuestViewBase> guest_;
};

GuestViewBase::GuestViewBase(content::RenderFrameHost* owner_rfh)
    : owner_rfh_id_(owner_rfh->GetGlobalId()),
      browser_context_(owner_rfh->GetBrowserContext()),
      guest_instance_id_(GetGuestViewManager()->GetNextInstanceID()) {
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    WebContentsObserver::Observe(owner_web_contents());
  } else {
    owner_contents_observer_ = std::make_unique<OwnerContentsObserver>(
        weak_ptr_factory_.GetSafeRef(), owner_web_contents());
  }
  SetOwnerHost();
}

GuestViewBase::~GuestViewBase() {
  DCHECK(!is_being_destroyed_);
  is_being_destroyed_ = true;

  // If `this` was ever attached, it is important to clear `owner_rfh_id_`
  // after the call to StopTrackingEmbedderZoomLevel(), but before the rest of
  // the statements in this function.
  StopTrackingEmbedderZoomLevel();
  owner_rfh_id_ = content::GlobalRenderFrameHostId();

  // This is not necessarily redundant with the removal when the guest contents
  // is destroyed, since we may never have initialized a guest WebContents.
  GetGuestViewManager()->RemoveGuest(this,
                                     /*invalidate_id=*/true);

  pending_events_.clear();
}

void GuestViewBase::Init(std::unique_ptr<GuestViewBase> owned_this,
                         scoped_refptr<content::SiteInstance> site_instance,
                         const base::Value::Dict& create_params,
                         GuestCreatedCallback callback) {
  if (!GetGuestViewManager()->IsGuestAvailableToContext(this)) {
    // The derived class did not create a WebContents so this class serves no
    // purpose. Let's self-destruct.
    owned_this.reset();
    std::move(callback).Run(nullptr);
    return;
  }

  CreateInnerPage(std::move(owned_this), site_instance, create_params,
                  base::BindOnce(&GuestViewBase::CompleteInit,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 create_params.Clone(), std::move(callback)));
}

void GuestViewBase::InitWithWebContents(const base::Value::Dict& create_params,
                                        WebContents* guest_web_contents) {
  CHECK(guest_web_contents);

  // Create a ZoomController to allow the guest's contents to be zoomed.
  // Do this before adding the GuestView as a WebContents Observer so that
  // the GuestView and its derived classes can re-configure the ZoomController
  // after the latter has handled WebContentsObserver events (observers are
  // notified of events in the same order they are added as observers). For
  // example, GuestViewBase may wish to put its guest into isolated zoom mode
  // in DidFinishNavigation, but since ZoomController always resets to default
  // zoom mode on this event, GuestViewBase would need to do so after
  // ZoomController::DidFinishNavigation has completed.
  zoom::ZoomController::CreateForWebContents(guest_web_contents);

  WebContentsObserver::Observe(guest_web_contents);
  guest_web_contents->SetDelegate(this);
  GetGuestViewManager()->AddGuest(this);

  // Populate the view instance ID if we have it on creation.
  view_instance_id_ =
      create_params.FindInt(kParameterInstanceId).value_or(view_instance_id_);

  SetUpSizing(create_params);

  // Observe guest zoom changes.
  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents());
  zoom_controller_observations_.AddObservation(zoom_controller);

  // Give the derived class an opportunity to perform additional initialization.
  DidInitialize(create_params);
}

void GuestViewBase::SetGuestPageHolder(
    content::GuestPageHolder* guest_page_holder) {
  CHECK(guest_page_holder);

  guest_main_frame_tree_node_id_ =
      guest_page_holder->GetGuestMainFrame()->GetFrameTreeNodeId();
  guest_page_ = guest_page_holder;
  CHECK_EQ(content::WebContents::FromFrameTreeNodeId(
               guest_main_frame_tree_node_id()),
           owner_web_contents());
}

void GuestViewBase::InitWithGuestPageHolder(
    const base::Value::Dict& create_params,
    content::GuestPageHolder* guest_page_holder) {
  SetGuestPageHolder(guest_page_holder);

  GetGuestViewManager()->AddGuest(this);

  // Populate the view instance ID if we have it on creation.
  view_instance_id_ =
      create_params.FindInt(kParameterInstanceId).value_or(view_instance_id_);

  SetUpSizing(create_params);

  // Observe guest zoom changes.
  auto* zoom_controller =
      zoom::ZoomController::CreateForWebContentsAndRenderFrameHost(
          web_contents(),
          guest_page_holder->GetGuestMainFrame()->GetGlobalId());
  CHECK(zoom_controller);
  CHECK(!zoom_controller_observations_.IsObservingSource(zoom_controller));
  zoom_controller_observations_.AddObservation(zoom_controller);

  // Give the derived class an opportunity to perform additional initialization.
  DidInitialize(create_params);
}

const std::optional<
    std::pair<base::Value::Dict, content::WebContents::CreateParams>>&
GuestViewBase::GetCreateParams() const {
  return create_params_;
}

void GuestViewBase::SetCreateParams(
    const base::Value::Dict& create_params,
    const content::WebContents::CreateParams& web_contents_create_params) {
  DCHECK_EQ(web_contents_create_params.browser_context, browser_context());
  DCHECK_EQ(web_contents_create_params.guest_delegate, this);
  create_params_ = {create_params.Clone(), web_contents_create_params};
}

zoom::ZoomController* GuestViewBase::GetZoomController() const {
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    return zoom::ZoomController::FromWebContentsAndRenderFrameHost(
        web_contents(), GetGuestMainFrame()->GetGlobalId());
  }
  return zoom::ZoomController::FromWebContents(web_contents());
}

void GuestViewBase::DispatchOnResizeEvent(const gfx::Size& old_size,
                                          const gfx::Size& new_size) {
  if (new_size == old_size)
    return;

  // Dispatch the onResize event.
  base::Value::Dict args;
  args.Set(kOldWidth, old_size.width());
  args.Set(kOldHeight, old_size.height());
  args.Set(kNewWidth, new_size.width());
  args.Set(kNewHeight, new_size.height());
  DispatchEventToGuestProxy(
      std::make_unique<GuestViewEvent>(kEventResize, std::move(args)));
}

gfx::Size GuestViewBase::GetDefaultSize() const {
  if (!is_full_page_plugin())
    return gfx::Size(kDefaultWidth, kDefaultHeight);

  // Full page plugins default to the size of the owner's viewport.
  return owner_rfh()->GetView()->GetVisibleViewportSize();
}

void GuestViewBase::SetSize(const SetSizeParams& params) {
  bool enable_auto_size = params.enable_auto_size.value_or(auto_size_enabled_);
  gfx::Size min_size = params.min_size.value_or(min_auto_size_);
  gfx::Size max_size = params.max_size.value_or(max_auto_size_);

  if (params.normal_size)
    normal_size_ = *params.normal_size;

  min_auto_size_ = min_size;
  min_auto_size_.SetToMin(max_size);
  max_auto_size_ = max_size;
  max_auto_size_.SetToMax(min_size);

  enable_auto_size &= !min_auto_size_.IsEmpty() && !max_auto_size_.IsEmpty() &&
                      IsAutoSizeSupported();

  content::RenderWidgetHostView* rwhv = GetGuestMainFrame()->GetView();
  if (enable_auto_size) {
    // Autosize is being enabled.
    if (rwhv)
      rwhv->EnableAutoResize(min_auto_size_, max_auto_size_);
    normal_size_.SetSize(0, 0);
  } else {
    // Autosize is being disabled.
    // Use default width/height if missing from partially defined normal size.
    if (normal_size_.width() && !normal_size_.height())
      normal_size_.set_height(GetDefaultSize().height());
    if (!normal_size_.width() && normal_size_.height())
      normal_size_.set_width(GetDefaultSize().width());

    gfx::Size new_size;
    if (!normal_size_.IsEmpty()) {
      new_size = normal_size_;
    } else if (!guest_size_.IsEmpty()) {
      new_size = guest_size_;
    } else {
      new_size = GetDefaultSize();
    }

    bool changed_due_to_auto_resize = false;
    if (auto_size_enabled_) {
      // Autosize was previously enabled.
      if (rwhv)
        rwhv->DisableAutoResize(new_size);
      changed_due_to_auto_resize = true;
    } else {
      // Autosize was already disabled. The RenderWidgetHostView is responsible
      // for the GuestView's size.
    }

    UpdateGuestSize(new_size, changed_due_to_auto_resize);
  }

  auto_size_enabled_ = enable_auto_size;
}

// static
GuestViewBase* GuestViewBase::FromWebContents(WebContents* web_contents) {
  CHECK(!base::FeatureList::IsEnabled(features::kGuestViewMPArch));
  if (!web_contents) {
    return nullptr;
  }

  auto* manager =
      GuestViewManager::FromBrowserContext(web_contents->GetBrowserContext());
  return manager ? manager->GetGuestFromWebContents(web_contents) : nullptr;
}

// static
GuestViewBase* GuestViewBase::FromRenderFrameHost(
    content::RenderFrameHost* rfh) {
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    if (!rfh) {
      return nullptr;
    }
    auto* manager =
        GuestViewManager::FromBrowserContext(rfh->GetBrowserContext());
    return manager ? manager->GetGuestFromRenderFrameHost(*rfh) : nullptr;
  } else {
    return FromWebContents(content::WebContents::FromRenderFrameHost(rfh));
  }
}

// static
GuestViewBase* GuestViewBase::FromRenderFrameHostId(
    const content::GlobalRenderFrameHostId& rfh_id) {
  return FromRenderFrameHost(content::RenderFrameHost::FromID(rfh_id));
}

// static
GuestViewBase* GuestViewBase::FromNavigationHandle(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle) {
    return nullptr;
  }
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    auto* manager = GuestViewManager::FromBrowserContext(
        navigation_handle->GetWebContents()->GetBrowserContext());
    return manager ? manager->GetGuestFromNavigationHandle(*navigation_handle)
                   : nullptr;
  } else {
    return FromWebContents(navigation_handle->GetWebContents());
  }
}

// static
GuestViewBase* GuestViewBase::FromFrameTreeNodeId(
    content::FrameTreeNodeId frame_tree_node_id) {
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    content::WebContents* web_contents =
        content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
    if (!web_contents) {
      return nullptr;
    }
    auto* manager =
        GuestViewManager::FromBrowserContext(web_contents->GetBrowserContext());
    return manager ? manager->GetGuestFromFrameTreeNodeId(frame_tree_node_id)
                   : nullptr;
  } else {
    return FromWebContents(
        content::WebContents::FromFrameTreeNodeId(frame_tree_node_id));
  }
}

// static
GuestViewBase* GuestViewBase::FromInstanceID(
    content::ChildProcessId owner_process_id,
    int guest_instance_id) {
  auto* host = content::RenderProcessHost::FromID(owner_process_id);
  if (!host) {
    return nullptr;
  }

  return GuestViewManager::FromBrowserContext(host->GetBrowserContext())
      ->GetGuestByInstanceIDSafely(guest_instance_id, owner_process_id);
}

GuestViewBase* GuestViewBase::FromInstanceID(int owner_process_id,
                                             int guest_instance_id) {
  auto* host = content::RenderProcessHost::FromID(owner_process_id);
  if (!host)
    return nullptr;

  return GuestViewManager::FromBrowserContext(host->GetBrowserContext())
      ->GetGuestByInstanceIDSafely(guest_instance_id, owner_process_id);
}

// static
WebContents* GuestViewBase::GetTopLevelWebContents(WebContents* web_contents) {
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    return web_contents;
  } else {
    while (GuestViewBase* guest = FromWebContents(web_contents)) {
      content::WebContents* owner_web_contents = guest->owner_web_contents();
      // If the embedder contents is shutdown before guest attachment,
      // `owner_web_contents()` will be null.
      // This is seen in WebViewTest.ShutdownBeforeAttach.
      if (!owner_web_contents) {
        break;
      }
      web_contents = owner_web_contents;
    }
    return web_contents;
  }
}

// static
bool GuestViewBase::IsGuest(WebContents* web_contents) {
  return !!FromWebContents(web_contents);
}

// static
bool GuestViewBase::IsGuest(content::RenderFrameHost* rfh) {
  return !!FromRenderFrameHost(rfh);
}

// static
bool GuestViewBase::IsGuest(const content::GlobalRenderFrameHostId& rfh_id) {
  return !!FromRenderFrameHostId(rfh_id);
}

// static
bool GuestViewBase::IsGuest(content::NavigationHandle* navigation_handle) {
  return !!FromNavigationHandle(navigation_handle);
}

// static
bool GuestViewBase::IsGuest(content::FrameTreeNodeId frame_tree_node_id) {
  return !!FromFrameTreeNodeId(frame_tree_node_id);
}

bool GuestViewBase::IsAutoSizeSupported() const {
  return false;
}

bool GuestViewBase::IsPreferredSizeModeEnabled() const {
  return false;
}

bool GuestViewBase::ZoomPropagatesFromEmbedderToGuest() const {
  return true;
}

content::NavigationController& GuestViewBase::GetController() {
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    return guest_page_->GetController();
  } else {
    return web_contents()->GetController();
  }
}

content::GuestPageHolder& GuestViewBase::GetGuestPageHolder() {
  CHECK(base::FeatureList::IsEnabled(features::kGuestViewMPArch));
  CHECK(guest_page_);
  return *guest_page_;
}

GuestViewManager* GuestViewBase::GetGuestViewManager() const {
  return GuestViewManager::FromBrowserContext(browser_context());
}

std::unique_ptr<WebContents> GuestViewBase::CreateNewGuestWindow(
    const WebContents::CreateParams& create_params) {
  return GetGuestViewManager()->CreateGuestWithWebContentsParams(
      std::string(GetViewType()), owner_rfh(), create_params);
}

void GuestViewBase::DidAttach() {
  DCHECK(attach_in_progress_);
  // Clear this flag here, as functions called below may check attached().
  attach_in_progress_ = false;

  opener_lifetime_observer_.reset();

  SetUpSizing(attach_params());

  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    NOTIMPLEMENTED();
  } else {
    // The guest should have the same muting state as the owner.
    web_contents()->SetAudioMuted(owner_web_contents()->IsAudioMuted());
  }

  // Give the derived class an opportunity to perform some actions.
  DidAttachToEmbedder();

  SendQueuedEvents();
}

WebContents* GuestViewBase::GetOwnerWebContents() {
  return owner_web_contents();
}

content::RenderFrameHost* GuestViewBase::GetProspectiveOuterDocument() {
  DCHECK(!attached());
  return owner_rfh();
}

const GURL& GuestViewBase::GetOwnerLastCommittedURL() const {
  return owner_rfh()->GetLastCommittedURL();
}

const GURL& GuestViewBase::GetOwnerSiteURL() const {
  return owner_rfh()->GetSiteInstance()->GetSiteURL();
}

void GuestViewBase::SetAttachParams(const base::Value::Dict& params) {
  attach_params_ = params.Clone();
  view_instance_id_ =
      attach_params_.FindInt(kParameterInstanceId).value_or(view_instance_id_);
}

void GuestViewBase::SetOpener(GuestViewBase* guest) {
  DCHECK(guest);
  opener_ = guest->weak_ptr_factory_.GetWeakPtr();
  if (!attached()) {
    opener_lifetime_observer_ = std::make_unique<OpenerLifetimeObserver>(this);
  }
}

void GuestViewBase::AttachToOuterWebContentsFrame(
    std::unique_ptr<GuestViewBase> owned_this,
    content::RenderFrameHost* outer_contents_frame,
    int element_instance_id,
    bool is_full_page_plugin,
    GuestViewMessageHandler::AttachToEmbedderFrameCallback
        attachment_callback) {
  // Stop tracking the old embedder's zoom level.
  // TODO(crbug.com/40436245): We should assert that we're not tracking the
  // embedder at this point, since guest reattachment is no longer possible.
  StopTrackingEmbedderZoomLevel();

  content::WebContents* embedder_web_contents =
      content::WebContents::FromRenderFrameHost(outer_contents_frame);

  if (owner_web_contents() != embedder_web_contents) {
    UpdateWebContentsForNewOwner(outer_contents_frame->GetParent());
  } else {
    // Even if the owner WebContents hasn't changed, it still could be the case
    // that the owner switches to a same-origin subframe. But we don't need to
    // do anything beyond updating the id of the owner in this case.
    owner_rfh_id_ = outer_contents_frame->GetParent()->GetGlobalId();
  }

  // Start tracking the new embedder's zoom level.
  StartTrackingEmbedderZoomLevel();
  attach_in_progress_ = true;
  element_instance_id_ = element_instance_id;
  is_full_page_plugin_ = is_full_page_plugin;

  WillAttachToEmbedder();

  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    // TODO(crbug.com/40202416): When implementing newwindow, resume loading of
    // the guest.

    // From this point on, `this` is scoped to the guest page's lifetime.
    std::unique_ptr<content::GuestPageHolder> owned_guest_page =
        std::move(owned_guest_page_);
    CHECK(
        !owned_guest_page->GetUserData(kGuestPageDelegateOwnershipUserDataKey));
    owned_guest_page->SetUserData(kGuestPageDelegateOwnershipUserDataKey,
                                  std::move(owned_this));

    owner_web_contents()->AttachGuestPage(std::move(owned_guest_page),
                                          outer_contents_frame);
    // TODO(crbug.com/40202416): Set focus according to `is_full_page_plugin`.
  } else {
    web_contents()->ResumeLoadingCreatedWebContents();

    // From this point on, `this` is scoped to the guest contents' lifetime. We
    // self-destruct in WebContentsDestroyed.
    owned_this.release();
    self_owned_ = true;
    std::unique_ptr<WebContents> owned_guest_contents =
        std::move(owned_guest_contents_);
    DCHECK_EQ(owned_guest_contents.get(), web_contents());
    if (owned_guest_contents) {
      owned_guest_contents->SetOwnerLocationForDebug(std::nullopt);
    }

    owner_web_contents()->AttachInnerWebContents(
        std::move(owned_guest_contents), outer_contents_frame,
        is_full_page_plugin);
  }

  // We don't ACK until after AttachToOuterWebContentsFrame, so that
  // |outer_contents_frame| gets swapped before the AttachToEmbedderFrame
  // callback is run. We also need to send the ACK before queued events are sent
  // in DidAttach.
  if (attachment_callback)
    std::move(attachment_callback).Run();

  // Completing attachment will resume suspended resource loads and then send
  // queued events.
  SignalWhenReady(base::BindOnce(&GuestViewBase::DidAttach,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void GuestViewBase::OnOwnerAudioMutedStateUpdated(bool muted) {
  CHECK(web_contents());
  web_contents()->SetAudioMuted(muted);
}

void GuestViewBase::SignalWhenReady(base::OnceClosure callback) {
  // The default behavior is to call the |callback| immediately. Derived classes
  // can implement an alternative signal for readiness.
  std::move(callback).Run();
}

int GuestViewBase::LogicalPixelsToPhysicalPixels(double logical_pixels) const {
  DCHECK(logical_pixels >= 0);
  double zoom_factor = GetEmbedderZoomFactor();
  return lround(logical_pixels * zoom_factor);
}

double GuestViewBase::PhysicalPixelsToLogicalPixels(int physical_pixels) const {
  DCHECK(physical_pixels >= 0);
  double zoom_factor = GetEmbedderZoomFactor();
  return physical_pixels / zoom_factor;
}

void GuestViewBase::GuestDidStopLoading() {
  if (IsPreferredSizeModeEnabled()) {
    GetGuestMainFrame()->GetRenderViewHost()->EnablePreferredSizeMode();
  }

  GuestViewDidStopLoading();
}

void GuestViewBase::GuestDocumentOnLoadCompleted() {
  GuestViewDocumentOnLoadCompleted();
}

void GuestViewBase::GuestDidChangeLoadProgress(double progress) {
  GuestViewDidChangeLoadProgress(progress);
}

void GuestViewBase::GuestMainFrameProcessGone(base::TerminationStatus status) {
  GuestViewMainFrameProcessGone(status);
}

void GuestViewBase::GuestResizeDueToAutoResize(const gfx::Size& new_size) {
  UpdateGuestSize(new_size, auto_size_enabled_);
}

void GuestViewBase::GuestUpdateWindowPreferredSize(const gfx::Size& pref_size) {
  // In theory it's not necessary to check IsPreferredSizeModeEnabled() because
  // there will only be events if it was enabled in the first place. However,
  // something else may have turned on preferred size mode, so double check.
  if (IsPreferredSizeModeEnabled()) {
    OnPreferredSizeChanged(pref_size);
  }
}

content::GuestPageHolder* GuestViewBase::GuestCreateNewWindow(
    WindowOpenDisposition disposition,
    const GURL& url,
    const std::string& main_frame_name,
    content::RenderFrameHost* opener,
    scoped_refptr<content::SiteInstance> site_instance) {
  NOTREACHED();
}

void GuestViewBase::GuestOpenURL(
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {}

void GuestViewBase::GuestClose() {}

void GuestViewBase::GuestRequestMediaAccessPermission(
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  std::move(callback).Run(blink::mojom::StreamDevicesSet(),
                          blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED,
                          std::unique_ptr<content::MediaStreamUI>());
}

bool GuestViewBase::GuestCheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  return false;
}

void GuestViewBase::LoadProgressChanged(double progress) {
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    // The load state of the embedder does not affect the load state of the
    // guest.
    return;
  }
  GuestDidChangeLoadProgress(progress);
}

void GuestViewBase::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    // For MPArch we will get notification directly for the guest. Don't do
    // anything if the embedder process dies.
    return;
  }
  GuestMainFrameProcessGone(status);
}

content::JavaScriptDialogManager*
GuestViewBase::GuestGetJavascriptDialogManager() {
  return nullptr;
}

void GuestViewBase::DidStopLoading() {
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    // The load state of the embedder does not affect the load state of the
    // guest.
    return;
  }

  GuestDidStopLoading();
}

void GuestViewBase::DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    // The load state of the embedder does not affect the load state of the
    // guest.
    return;
  }

  GuestDocumentOnLoadCompleted();
}

void GuestViewBase::GuestOverrideRendererPreferences(
    blink::RendererPreferences& preferences) {}

void GuestViewBase::FrameDeleted(content::FrameTreeNodeId frame_tree_node_id) {
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch) &&
      web_contents()->IsBeingDestroyed()) {
    // If the primary frame tree is shutting down, we need to destroy any
    // unattached guest frame trees now. We can't wait until
    // WebContentsDestroyed to do this, otherwise other WebContentsObservers
    // will see a confusing sequence of events.
    ClearOwnedGuestPage();
  }
}

void GuestViewBase::WebContentsDestroyed() {
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    // Once attached, the guest can't outlive its owner WebContents.
    CHECK_EQ(element_instance_id(), kInstanceIDNone);

    // Defensively clear the guest's `owner_rfh_id_`, since a unique_ptr
    // to the guest may be passed through asynchronous calls early in its
    // initialization, and it's possible that the owner WebContents could be
    // destroyed during this process. Lookups using an id would still be safe,
    // but we clear this anyway to avoid unexpected lookups during destruction.
    owner_rfh_id_ = content::GlobalRenderFrameHostId();
    DestroyGuestIfUnattached(this);
  } else {
    GetGuestViewManager()->RemoveGuest(this,
                                       /*invalidate_id=*/false);

    // Self-destruct.
    if (self_owned_) {
      DCHECK(!is_being_destroyed_);
      delete this;
    }
  }
}

void GuestViewBase::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!IsObservedNavigationWithinGuestMainFrame(navigation_handle) ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  if (attached() && ZoomPropagatesFromEmbedderToGuest()) {
    SetGuestZoomLevelToMatchEmbedder();
  }
}

bool GuestViewBase::IsObservedNavigationWithinGuest(
    content::NavigationHandle* navigation_handle) {
  CHECK(navigation_handle);
  CHECK_EQ(navigation_handle->GetWebContents(), web_contents());
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    auto* navigating_guest =
        GuestViewBase::FromNavigationHandle(navigation_handle);
    return navigating_guest == this;
  } else {
    // Due to the use of inner WebContents, no navigations external to the guest
    // WebContents are observed.
    return true;
  }
}

bool GuestViewBase::IsObservedNavigationWithinGuestMainFrame(
    content::NavigationHandle* navigation_handle) {
  CHECK(navigation_handle);
  CHECK_EQ(navigation_handle->GetWebContents(), web_contents());
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    return navigation_handle->GetNavigatingFrameType() ==
               content::FrameType::kGuestMainFrame &&
           IsObservedNavigationWithinGuest(navigation_handle);
  } else {
    // Due to the use of inner WebContents, a guest's main frame is considered
    // primary.
    return navigation_handle->IsInPrimaryMainFrame();
  }
}

bool GuestViewBase::IsObservedRenderFrameHostWithinGuest(
    content::RenderFrameHost* rfh) {
  CHECK(rfh);
  CHECK_EQ(content::WebContents::FromRenderFrameHost(rfh), web_contents());
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    auto* guest = GuestViewBase::FromRenderFrameHost(rfh);
    return guest == this;
  } else {
    // Due to the use of inner WebContents, no RenderFrameHosts external to the
    // guest WebContents are observed.
    return true;
  }
}

void GuestViewBase::ActivateContents(WebContents* web_contents) {
  CHECK(!base::FeatureList::IsEnabled(features::kGuestViewMPArch));

  if (!attached() || !embedder_web_contents()->GetDelegate()) {
    return;
  }

  embedder_web_contents()->GetDelegate()->ActivateContents(
      embedder_web_contents());
}

void GuestViewBase::ContentsMouseEvent(WebContents* source,
                                       const ui::Event& event) {
  CHECK(!base::FeatureList::IsEnabled(features::kGuestViewMPArch));

  if (!attached() || !embedder_web_contents()->GetDelegate()) {
    return;
  }

  embedder_web_contents()->GetDelegate()->ContentsMouseEvent(
      embedder_web_contents(), event);
}

void GuestViewBase::ContentsZoomChange(bool zoom_in) {
  CHECK(!base::FeatureList::IsEnabled(features::kGuestViewMPArch));

  if (!attached() || !embedder_web_contents()->GetDelegate()) {
    return;
  }
  embedder_web_contents()->GetDelegate()->ContentsZoomChange(zoom_in);
}

bool GuestViewBase::HandleKeyboardEvent(
    WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  CHECK(!base::FeatureList::IsEnabled(features::kGuestViewMPArch));

  if (!attached() || !embedder_web_contents()->GetDelegate()) {
    return false;
  }

  // Send the keyboard events back to the embedder to reprocess them.
  return embedder_web_contents()->GetDelegate()->HandleKeyboardEvent(
      embedder_web_contents(), event);
}

void GuestViewBase::ResizeDueToAutoResize(WebContents* web_contents,
                                          const gfx::Size& new_size) {
  CHECK(!base::FeatureList::IsEnabled(features::kGuestViewMPArch));

  UpdateGuestSize(new_size, auto_size_enabled_);
}

void GuestViewBase::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  CHECK(!base::FeatureList::IsEnabled(features::kGuestViewMPArch));

  if (!attached() || !embedder_web_contents()->GetDelegate()) {
    listener->FileSelectionCanceled();
    return;
  }

  embedder_web_contents()->GetDelegate()->RunFileChooser(
      render_frame_host, std::move(listener), params);
}

bool GuestViewBase::ShouldFocusPageAfterCrash(content::WebContents* source) {
  CHECK(!base::FeatureList::IsEnabled(features::kGuestViewMPArch));

  // Focus is managed elsewhere.
  return false;
}

bool GuestViewBase::PreHandleGestureEvent(WebContents* source,
                                          const blink::WebGestureEvent& event) {
  CHECK(!base::FeatureList::IsEnabled(features::kGuestViewMPArch));
  // Pinch events which cause a scale change should not be routed to a guest.
  // We still allow synthetic wheel events for touchpad pinch to go to the page.
  DCHECK(!blink::WebInputEvent::IsPinchGestureEventType(event.GetType()) ||
         (event.SourceDevice() == blink::WebGestureDevice::kTouchpad &&
          event.NeedsWheelEvent()));
  return false;
}

void GuestViewBase::UpdatePreferredSize(WebContents* target_web_contents,
                                        const gfx::Size& pref_size) {
  CHECK(!base::FeatureList::IsEnabled(features::kGuestViewMPArch));
  DCHECK_EQ(web_contents(), target_web_contents);
  GuestUpdateWindowPreferredSize(pref_size);
}

void GuestViewBase::UpdateTargetURL(WebContents* source, const GURL& url) {
  CHECK(!base::FeatureList::IsEnabled(features::kGuestViewMPArch));

  if (!attached() || !embedder_web_contents()->GetDelegate()) {
    return;
  }

  embedder_web_contents()->GetDelegate()->UpdateTargetURL(
      embedder_web_contents(), url);
}

void GuestViewBase::OnZoomControllerDestroyed(zoom::ZoomController* source) {
  DCHECK(zoom_controller_observations_.IsObservingSource(source));
  zoom_controller_observations_.RemoveObservation(source);
}

void GuestViewBase::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  const bool embedder_zoom_changed =
      attached() &&
      (base::FeatureList::IsEnabled(features::kGuestViewMPArch)
           ? data.frame_tree_node_id ==
                 owner_rfh()->GetOutermostMainFrame()->GetFrameTreeNodeId()
           : data.web_contents == embedder_web_contents());
  if (embedder_zoom_changed) {
    // The embedder's zoom level has changed.
    auto* guest_zoom_controller = GetZoomController();
    if (blink::ZoomValuesEqual(data.new_zoom_level,
                               guest_zoom_controller->GetZoomLevel())) {
      return;
    }
    // When the embedder's zoom level doesn't match the guest's, then update the
    // guest's zoom level to match.
    guest_zoom_controller->SetZoomLevel(data.new_zoom_level);
    return;
  }

  const bool guest_zoom_changed =
      base::FeatureList::IsEnabled(features::kGuestViewMPArch)
          ? data.frame_tree_node_id == GetGuestMainFrame()->GetFrameTreeNodeId()
          : data.web_contents == web_contents();
  if (guest_zoom_changed) {
    // The guest's zoom level has changed.
    GuestZoomChanged(data.old_zoom_level, data.new_zoom_level);
    return;
  }

  // Note: we can get here if the event isn't for the guest, and
  // embedder_web_contents = nullptr. Not sure if this is a concern or not.
  // E.g. in browser_tests, DeveloperPrivateApiTest.InspectEmbeddedOptionsPage.
  // This seems to be a legacy issue.
}

void GuestViewBase::DispatchEventToGuestProxy(
    std::unique_ptr<GuestViewEvent> event) {
  event->Dispatch(this, guest_instance_id_);
}

void GuestViewBase::DispatchEventToView(std::unique_ptr<GuestViewEvent> event) {
  if (attached() && pending_events_.empty()) {
    event->Dispatch(this, view_instance_id_);
    return;
  }
  pending_events_.push_back(std::move(event));
}

void GuestViewBase::SendQueuedEvents() {
  if (!attached())
    return;
  while (!pending_events_.empty()) {
    std::unique_ptr<GuestViewEvent> event_ptr =
        std::move(pending_events_.front());
    pending_events_.pop_front();
    event_ptr->Dispatch(this, view_instance_id_);
  }
}

void GuestViewBase::RejectGuestCreation(
    std::unique_ptr<GuestViewBase> owned_this,
    GuestPageCreatedCallback callback) {
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    std::move(callback).Run(std::move(owned_this),
                            std::unique_ptr<content::GuestPageHolder>{nullptr});
  } else {
    std::move(callback).Run(std::move(owned_this),
                            std::unique_ptr<content::WebContents>{nullptr});
  }
}

void GuestViewBase::CompleteInit(base::Value::Dict create_params,
                                 GuestCreatedCallback callback,
                                 std::unique_ptr<GuestViewBase> owned_this,
                                 GuestPageVariant guest_page) {
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    CHECK(std::holds_alternative<std::unique_ptr<content::GuestPageHolder>>(
        guest_page));
    std::unique_ptr<content::GuestPageHolder> guest_page_holder =
        std::get<std::unique_ptr<content::GuestPageHolder>>(
            std::move(guest_page));
    if (!guest_page_holder) {
      // The derived class did not create a guest page so this class
      // serves no purpose. Let's self-destruct.
      owned_this.reset();
      std::move(callback).Run(nullptr);
      return;
    }
    InitWithGuestPageHolder(create_params, guest_page_holder.get());
    TakeGuestPageOwnership(std::move(guest_page_holder));
    std::move(callback).Run(std::move(owned_this));
  } else {
    CHECK(std::holds_alternative<std::unique_ptr<content::WebContents>>(
        guest_page));
    std::unique_ptr<content::WebContents> guest_web_contents =
        std::get<std::unique_ptr<content::WebContents>>(std::move(guest_page));
    if (!guest_web_contents) {
      // The derived class did not create a guest WebContents so this class
      // serves no purpose. Let's self-destruct.
      owned_this.reset();
      std::move(callback).Run(nullptr);
      return;
    }
    InitWithWebContents(create_params, guest_web_contents.get());
    TakeGuestContentsOwnership(std::move(guest_web_contents));
    std::move(callback).Run(std::move(owned_this));
  }
}

void GuestViewBase::TakeGuestContentsOwnership(
    std::unique_ptr<WebContents> guest_web_contents) {
  DCHECK(!owned_guest_contents_);
  owned_guest_contents_ = std::move(guest_web_contents);
  if (owned_guest_contents_) {
    owned_guest_contents_->SetOwnerLocationForDebug(FROM_HERE);
  }
}

void GuestViewBase::ClearOwnedGuestContents() {
  owned_guest_contents_.reset();
}

void GuestViewBase::TakeGuestPageOwnership(
    std::unique_ptr<content::GuestPageHolder> guest_page_holder) {
  CHECK(!owned_guest_page_);
  CHECK(guest_page_);
  CHECK_EQ(guest_page_holder.get(), guest_page_);
  owned_guest_page_ = std::move(guest_page_holder);
}

void GuestViewBase::ClearOwnedGuestPage() {
  if (owned_guest_page_) {
    CHECK_EQ(owned_guest_page_.get(), guest_page_);
    guest_page_ = nullptr;
    owned_guest_page_.reset();
  }
}

void GuestViewBase::UpdateWebContentsForNewOwner(
    content::RenderFrameHost* new_owner_rfh) {
  content::WebContents* new_owner_web_contents =
      content::WebContents::FromRenderFrameHost(new_owner_rfh);
  DCHECK(!attached());
  DCHECK(owner_web_contents());
  DCHECK(new_owner_web_contents);
  DCHECK_NE(owner_web_contents(), new_owner_web_contents);
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    DCHECK_EQ(web_contents(), owner_web_contents());
  } else {
    DCHECK_EQ(owner_contents_observer_->web_contents(), owner_web_contents());
  }

  owner_rfh_id_ = new_owner_rfh->GetGlobalId();

  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    WebContentsObserver::Observe(owner_web_contents());
  } else {
    owner_contents_observer_ = std::make_unique<OwnerContentsObserver>(
        weak_ptr_factory_.GetSafeRef(), owner_web_contents());
  }
  SetOwnerHost();
}

double GuestViewBase::GetEmbedderZoomFactor() const {
  if (!embedder_web_contents())
    return 1.0;

  return blink::ZoomLevelToZoomFactor(
      zoom::ZoomController::GetZoomLevelForWebContents(
          embedder_web_contents()));
}

void GuestViewBase::SetUpSizing(const base::Value::Dict& params) {
  // Read the autosize parameters passed in from the embedder.
  std::optional<bool> auto_size_enabled_opt =
      params.FindBool(kAttributeAutoSize);
  bool auto_size_enabled = auto_size_enabled_opt.value_or(auto_size_enabled_);

  int max_height =
      params.FindInt(kAttributeMaxHeight).value_or(max_auto_size_.height());
  int max_width =
      params.FindInt(kAttributeMaxWidth).value_or(max_auto_size_.width());

  int min_height =
      params.FindInt(kAttributeMinHeight).value_or(min_auto_size_.height());
  int min_width =
      params.FindInt(kAttributeMinWidth).value_or(min_auto_size_.width());

  double element_height = params.FindDouble(kElementHeight).value_or(0.0);
  double element_width = params.FindDouble(kElementWidth).value_or(0.0);

  // Set the normal size to the element size so that the guestview will fit
  // the element initially if autosize is disabled.
  int normal_height = normal_size_.height();
  int normal_width = normal_size_.width();
  // If the element size was provided in logical units (versus physical), then
  // it will be converted to physical units.
  std::optional<bool> element_size_is_logical_opt =
      params.FindBool(kElementSizeIsLogical);
  bool element_size_is_logical = element_size_is_logical_opt.value_or(false);
  if (element_size_is_logical) {
    // Convert the element size from logical pixels to physical pixels.
    normal_height = LogicalPixelsToPhysicalPixels(element_height);
    normal_width = LogicalPixelsToPhysicalPixels(element_width);
  } else {
    normal_height = lround(element_height);
    normal_width = lround(element_width);
  }

  SetSizeParams set_size_params;
  set_size_params.enable_auto_size = auto_size_enabled;
  set_size_params.min_size.emplace(min_width, min_height);
  set_size_params.max_size.emplace(max_width, max_height);
  set_size_params.normal_size.emplace(normal_width, normal_height);

  // Call SetSize to apply all the appropriate validation and clipping of
  // values.
  SetSize(set_size_params);
}

void GuestViewBase::SetGuestZoomLevelToMatchEmbedder() {
  auto* embedder_zoom_controller =
      zoom::ZoomController::FromWebContentsAndRenderFrameHost(
          owner_web_contents(),
          owner_rfh()->GetOutermostMainFrame()->GetGlobalId());
  if (!embedder_zoom_controller)
    return;

  GetZoomController()->SetZoomLevel(embedder_zoom_controller->GetZoomLevel());
}

void GuestViewBase::StartTrackingEmbedderZoomLevel() {
  if (!ZoomPropagatesFromEmbedderToGuest())
    return;

  auto* embedder_zoom_controller =
      zoom::ZoomController::FromWebContentsAndRenderFrameHost(
          owner_web_contents(),
          owner_rfh()->GetOutermostMainFrame()->GetGlobalId());

  // Chrome Apps do not have a ZoomController.
  if (!embedder_zoom_controller)
    return;
  // Listen to the embedder's zoom changes.
  zoom_controller_observations_.AddObservation(embedder_zoom_controller);

  // Set the guest's initial zoom level to be equal to the embedder's.
  SetGuestZoomLevelToMatchEmbedder();
}

void GuestViewBase::StopTrackingEmbedderZoomLevel() {
  // TODO(wjmaclean): Remove the observer any time the GuestWebView transitions
  // from propagating to not-propagating the zoom from the embedder.

  if (!owner_web_contents())
    return;
  auto* embedder_zoom_controller =
      zoom::ZoomController::FromWebContentsAndRenderFrameHost(
          owner_web_contents(),
          owner_rfh()->GetOutermostMainFrame()->GetGlobalId());
  // Chrome Apps do not have a ZoomController.
  if (!embedder_zoom_controller)
    return;

  if (zoom_controller_observations_.IsObservingSource(
          embedder_zoom_controller)) {
    zoom_controller_observations_.RemoveObservation(embedder_zoom_controller);
  }
}

void GuestViewBase::UpdateGuestSize(const gfx::Size& new_size,
                                    bool due_to_auto_resize) {
  if (due_to_auto_resize)
    GuestSizeChangedDueToAutoSize(guest_size_, new_size);
  DispatchOnResizeEvent(guest_size_, new_size);
  guest_size_ = new_size;
}

bool GuestViewBase::IsOwnedByExtension() const {
  return GetGuestViewManager()->IsOwnedByExtension(this);
}

bool GuestViewBase::IsOwnedByWebUI() const {
  return owner_rfh()->GetMainFrame()->GetWebUI();
}

bool GuestViewBase::IsOwnedByControlledFrameEmbedder() const {
  return GetGuestViewManager()->IsOwnedByControlledFrameEmbedder(this);
}

void GuestViewBase::SetOwnerHost() {
  if (IsOwnedByExtension()) {
    owner_host_ = GetOwnerLastCommittedURL().host();
  } else if (IsOwnedByWebUI()) {
    owner_host_ = std::string();
  } else if (IsOwnedByControlledFrameEmbedder()) {
    owner_host_ = owner_rfh()->GetLastCommittedOrigin().Serialize();
  } else {
    owner_host_ = std::string();
  }
  return;
}

bool GuestViewBase::CanBeEmbeddedInsideCrossProcessFrames() const {
  return false;
}

bool GuestViewBase::RequiresSslInterstitials() const {
  return false;
}

bool GuestViewBase::IsPermissionRequestable(ContentSettingsType type) const {
  return true;
}

std::optional<content::PermissionResult>
GuestViewBase::OverridePermissionResult(ContentSettingsType type) const {
  return std::nullopt;
}

content::RenderFrameHost* GuestViewBase::GetGuestMainFrame() const {
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    if (!guest_page_) {
      return nullptr;
    }
    CHECK_EQ(guest_page_->GetGuestMainFrame(),
             owner_web_contents()->UnsafeFindFrameByFrameTreeNodeId(
                 guest_main_frame_tree_node_id()));
    return guest_page_->GetGuestMainFrame();
  } else {
    return web_contents()->GetPrimaryMainFrame();
  }
}

content::FrameTreeNodeId GuestViewBase::guest_main_frame_tree_node_id() const {
  CHECK(base::FeatureList::IsEnabled(features::kGuestViewMPArch));
  return guest_main_frame_tree_node_id_;
}

content::GuestPageHolder* GuestViewBase::owned_guest_page() const {
  CHECK(base::FeatureList::IsEnabled(features::kGuestViewMPArch));
  return owned_guest_page_.get();
}

base::WeakPtr<content::BrowserPluginGuestDelegate>
GuestViewBase::GetGuestDelegateWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<content::GuestPageHolder::Delegate>
GuestViewBase::GetGuestPageHolderDelegateWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace guest_view
