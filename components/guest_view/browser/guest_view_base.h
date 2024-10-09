// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_BASE_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_BASE_H_

#include <memory>
#include <optional>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/guest_view/browser/guest_view_message_handler.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "components/zoom/zoom_observer.h"
#include "content/public/browser/browser_plugin_guest_delegate.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}

namespace guest_view {

class GuestViewEvent;
class GuestViewManager;

// A struct of parameters for SetSize(). The parameters are all declared as
// optionals. Nullopts indicate that the parameter has not been provided, and
// the last used value should be used. Note that when |enable_auto_size| is
// true, providing |normal_size| is not meaningful. This is because the normal
// size of the guestview is overridden whenever autosizing occurs.
struct SetSizeParams {
  SetSizeParams();
  ~SetSizeParams();

  std::optional<bool> enable_auto_size;
  std::optional<gfx::Size> min_size;
  std::optional<gfx::Size> max_size;
  std::optional<gfx::Size> normal_size;
};

// A GuestViewBase is the base class browser-side API implementation for a
// <*view> tag. GuestViewBase maintains an association between a guest
// WebContents and an owner WebContents. It receives events issued from
// the guest and relays them to the owner. GuestViewBase tracks the lifetime
// of its owner. A GuestViewBase's owner is referred to as an embedder if
// it is attached to a container within the owner's WebContents.
class GuestViewBase : public content::BrowserPluginGuestDelegate,
                      public content::WebContentsDelegate,
                      public content::WebContentsObserver,
                      public zoom::ZoomObserver {
 public:
  static GuestViewBase* FromInstanceID(int owner_process_id, int instance_id);

  // Prefer using FromRenderFrameHost. See https://crbug.com/1362569.
  static GuestViewBase* FromWebContents(content::WebContents* web_contents);

  static GuestViewBase* FromRenderFrameHost(content::RenderFrameHost* rfh);
  static GuestViewBase* FromRenderFrameHostId(
      const content::GlobalRenderFrameHostId& rfh_id);

  static GuestViewBase* FromNavigationHandle(
      content::NavigationHandle* navigation_handle);
  static GuestViewBase* FromFrameTreeNodeId(
      content::FrameTreeNodeId frame_tree_node_id);

  ~GuestViewBase() override;
  GuestViewBase(const GuestViewBase&) = delete;
  GuestViewBase& operator=(const GuestViewBase&) = delete;

  using WebContentsCreatedCallback = base::OnceCallback<void(
      std::unique_ptr<GuestViewBase> guest,
      std::unique_ptr<content::WebContents> guest_contents)>;

  // Given a |web_contents|, returns the top level owner WebContents. If
  // |web_contents| does not belong to a GuestView, it will be returned
  // unchanged.
  static content::WebContents* GetTopLevelWebContents(
      content::WebContents* web_contents);

  static bool IsGuest(content::WebContents* web_contents);
  static bool IsGuest(content::RenderFrameHost* rfh);
  static bool IsGuest(const content::GlobalRenderFrameHostId& rfh_id);
  static bool IsGuest(content::NavigationHandle* navigation_handle);
  static bool IsGuest(content::FrameTreeNodeId frame_tree_node_id);

  // Returns the name of the derived type of this GuestView.
  virtual const char* GetViewType() const = 0;

  // This method queries whether autosize is supported for this particular view.
  // By default, autosize is not supported. Derived classes can override this
  // behavior to support autosize.
  virtual bool IsAutoSizeSupported() const;

  // This method queries whether preferred size events are enabled for this
  // view. By default, preferred size events are disabled, since they add a
  // small amount of overhead.
  virtual bool IsPreferredSizeModeEnabled() const;

  // This indicates whether zoom should propagate from the embedder to the guest
  // content.
  virtual bool ZoomPropagatesFromEmbedderToGuest() const;

  // Access to guest views are determined by the availability of the internal
  // extension API used to implement the guest view.
  //
  // This should be the name of the API as it appears in the _api_features.json
  // file.
  virtual const char* GetAPINamespace() const = 0;

  // This method is the task prefix to show for a task produced by this
  // GuestViewBase's derived type.
  virtual int GetTaskPrefix() const = 0;

  // Dispatches an event to the guest proxy.
  void DispatchEventToGuestProxy(std::unique_ptr<GuestViewEvent> event);

  // Dispatches an event to the view.
  void DispatchEventToView(std::unique_ptr<GuestViewEvent> event);

  // This creates a WebContents and initializes |this| GuestViewBase to use the
  // newly created WebContents.
  using GuestCreatedCallback =
      base::OnceCallback<void(std::unique_ptr<GuestViewBase> guest)>;
  void Init(std::unique_ptr<GuestViewBase> owned_this,
            const base::Value::Dict& create_params,
            GuestCreatedCallback callback);

  void InitWithWebContents(const base::Value::Dict& create_params,
                           content::WebContents* guest_web_contents);

  void SetCreateParams(
      const base::Value::Dict& create_params,
      const content::WebContents::CreateParams& web_contents_create_params);

  // As part of the migration of GuestViews to MPArch, we need to know what the
  // embedder WebContents is at the time that the guest page is created.
  // <webview>s have an edge case where we have to create the guest page before
  // then. We assume the owner doesn't change before attachment, but if it does,
  // we destroy and recreate the guest page. See this doc for details:
  // https://docs.google.com/document/d/1RVbtvklXUg9QCNvMT0r-1qDwJNeQFGoTCOD1Ur9mDa4/edit?usp=sharing
  //
  // `outer_contents_frame` is the frame at which the guest will be attached.
  // `outer_contents_frame` is in a different WebContents from the `owner_rfh()`
  // at the time of calling this. `outer_contents_frame`'s parent will be the
  // new owner RenderFrameHost.
  virtual void MaybeRecreateGuestContents(
      content::RenderFrameHost* outer_contents_frame) = 0;

  // Used to toggle autosize mode for this GuestView, and set both the automatic
  // and normal sizes.
  void SetSize(const SetSizeParams& params);

  // See the class comment for the difference between "owner" and "embedder."
  content::WebContents* embedder_web_contents() const {
    return attached() ? owner_web_contents() : nullptr;
  }

  content::WebContents* owner_web_contents() const {
    return content::WebContents::FromRenderFrameHost(owner_rfh());
  }

  // See the class comment for the difference between "owner" and "embedder."
  content::RenderFrameHost* embedder_rfh() const {
    return attached() ? owner_rfh() : nullptr;
  }

  content::RenderFrameHost* owner_rfh() const {
    return content::RenderFrameHost::FromID(owner_rfh_id_);
  }

  // Returns the parameters associated with the element hosting this GuestView
  // passed in from JavaScript.
  const base::Value::Dict& attach_params() const { return attach_params_; }

  // Returns whether this guest has an associated embedder.
  bool attached() const {
    return !(element_instance_id_ == kInstanceIDNone || attach_in_progress_ ||
             is_being_destroyed_);
  }

  // Returns the instance ID of the <*view> element.
  int view_instance_id() const { return view_instance_id_; }

  // Returns the instance ID of this GuestViewBase.
  int guest_instance_id() const { return guest_instance_id_; }

  // Returns the instance ID of the GuestViewBase's element (unique within an
  // embedder process). Note: this value is set once after attach is complete.
  // It will maintain its value during the lifetime of GuestViewBase, even after
  // |attach()| is false due to |is_being_destroyed_|.
  int element_instance_id() const { return element_instance_id_; }

  bool can_owner_receive_events() const { return !!view_instance_id_; }

  gfx::Size size() const { return guest_size_; }

  // Returns the user browser context of the embedder.
  content::BrowserContext* browser_context() const { return browser_context_; }

  content::NavigationController& GetController();

  GuestViewManager* GetGuestViewManager() const;

  // Returns the URL of the owner RenderFrameHost's last committed URL.
  const GURL& GetOwnerLastCommittedURL() const;

  // Returns the URL of the owner RenderFrameHost's SiteInstance.
  const GURL& GetOwnerSiteURL() const;

  // Returns the host of the owner WebContents. If the owner RenderFrameHost is
  // for an extension, returns the host of its URL, which is an extension ID. If
  // the owner RenderFrameHost is a non-extension embedder of a Controlled
  // Frame, returns its serialized origin.
  // TODO(crbug.com/41490369): Expose this information as a url::Origin.
  std::string owner_host() const { return owner_host_; }

  // Whether the guest view is inside a plugin document.
  bool is_full_page_plugin() const { return is_full_page_plugin_; }

  bool IsOwnedByExtension() const;
  bool IsOwnedByWebUI() const;
  bool IsOwnedByControlledFrameEmbedder() const;

  // Saves the attach state of the custom element hosting this GuestView.
  void SetAttachParams(const base::Value::Dict& params);

  // Starts the attaching process for a (frame-based) GuestView.
  // |outer_contents_frame| is a frame in the embedder WebContents (owned by a
  // HTMLFrameOwnerElement associated with the GuestView's element in the
  // embedder process) which will be used for attaching.
  void AttachToOuterWebContentsFrame(
      std::unique_ptr<GuestViewBase> owned_this,
      content::RenderFrameHost* outer_contents_frame,
      int element_instance_id,
      bool is_full_page_plugin,
      GuestViewMessageHandler::AttachToEmbedderFrameCallback
          attachment_callback);

  // Returns true if the corresponding guest is allowed to be embedded inside an
  // <iframe> which is cross process.
  virtual bool CanBeEmbeddedInsideCrossProcessFrames() const;

  // Returns true if an SSL error in the guest's main frame should show an
  // interstitial instead of a plain error page.
  virtual bool RequiresSslInterstitials() const;

  // Returns false if permission management should automatically drop
  // permission requests of the given `type`.
  virtual bool IsPermissionRequestable(ContentSettingsType type) const;

  // If the GuestView overrides |type|, returns the permission result of |type|.
  // Otherwise, returns nullopt.
  virtual std::optional<content::PermissionResult> OverridePermissionResult(
      ContentSettingsType type) const;

  content::RenderFrameHost* GetGuestMainFrame() const;

 protected:
  explicit GuestViewBase(content::RenderFrameHost* owner_rfh);

  GuestViewBase* GetOpener() const { return opener_.get(); }

  void SetOpener(GuestViewBase* opener);

  const std::optional<
      std::pair<base::Value::Dict, content::WebContents::CreateParams>>&
  GetCreateParams() const;

  // Convenience method for `CreateWebContents` implementations when not
  // creating a guest.
  void RejectGuestCreation(std::unique_ptr<GuestViewBase> owned_this,
                           WebContentsCreatedCallback callback);

  void TakeGuestContentsOwnership(
      std::unique_ptr<content::WebContents> guest_web_contents);
  void ClearOwnedGuestContents();

  // Called when the current `owner_rfh()` is in a different WebContents from
  // the frame that will be used for attachment. `owner_rfh` is the parent of
  // the RenderFrameHost that will be used for attachment.
  void UpdateWebContentsForNewOwner(content::RenderFrameHost* new_owner_rfh);

  // BrowserPluginGuestDelegate implementation.
  content::RenderFrameHost* GetProspectiveOuterDocument() override;

  // WebContentsDelegate implementation.
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;

  // WebContentsObserver implementation.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  // Given a set of initialization parameters, a concrete subclass of
  // GuestViewBase can create a specialized WebContents that it returns back to
  // GuestViewBase.
  virtual void CreateWebContents(std::unique_ptr<GuestViewBase> owned_this,
                                 const base::Value::Dict& create_params,
                                 WebContentsCreatedCallback callback) = 0;

  // This method is called after the guest has been attached to an embedder and
  // suspended resource loads have been resumed.
  //
  // This method can be overriden by subclasses. This gives the derived class
  // an opportunity to perform setup actions after attachment.
  virtual void DidAttachToEmbedder() {}

  // This method is called after this GuestViewBase has been initiated.
  //
  // This gives the derived class an opportunity to perform additional
  // initialization.
  virtual void DidInitialize(const base::Value::Dict& create_params) {}

  // This method is called when embedder WebContents's fullscreen is toggled.
  //
  // If the guest asked the embedder to enter fullscreen, the guest uses this
  // signal to exit fullscreen state.
  virtual void EmbedderFullscreenToggled(bool entered_fullscreen) {}

  // This method is called when the initial set of frames within the page have
  // completed loading.
  virtual void GuestViewDidStopLoading() {}

  // This method is called when the guest's zoom changes.
  virtual void GuestZoomChanged(double old_zoom_level, double new_zoom_level) {}

  // This method is invoked when the contents auto-resized to give the container
  // an opportunity to match it if it wishes.
  //
  // This gives the derived class an opportunity to inform its container element
  // or perform other actions.
  virtual void GuestSizeChangedDueToAutoSize(const gfx::Size& old_size,
                                             const gfx::Size& new_size) {}

  // This method is invoked when the contents preferred size changes. This will
  // only ever fire if IsPreferredSizeSupported returns true.
  virtual void OnPreferredSizeChanged(const gfx::Size& pref_size) {}

  // This method is invoked when the owner contents audio muted state changes to
  // give the container an opportunity to adjust their muted state.
  virtual void OnOwnerAudioMutedStateUpdated(bool muted);

  // Signals that the guest view is ready.  The default implementation signals
  // immediately, but derived class can override this if they need to do
  // asynchronous setup.
  virtual void SignalWhenReady(base::OnceClosure callback);

  // This method is called immediately before suspended resource loads have been
  // resumed on attachment to an embedder.
  //
  // This method can be overriden by subclasses. This gives the derived class
  // an opportunity to perform setup actions before attachment.
  virtual void WillAttachToEmbedder() {}

  // Convert sizes in pixels from logical to physical numbers of pixels.
  // Note that a size can consist of a fractional number of logical pixels
  // (hence |logical_pixels| is represented as a double), but will always
  // consist of an integral number of physical pixels (hence the return value
  // is represented as an int).
  int LogicalPixelsToPhysicalPixels(double logical_pixels) const;

  // Convert sizes in pixels from physical to logical numbers of pixels.
  // Note that a size can consist of a fractional number of logical pixels
  // (hence the return value is represented as a double), but will always
  // consist of an integral number of physical pixels (hence |physical_pixels|
  // is represented as an int).
  double PhysicalPixelsToLogicalPixels(int physical_pixels) const;

  void SetGuestZoomLevelToMatchEmbedder();

 private:
  class OwnerContentsObserver;
  class OpenerLifetimeObserver;

  // TODO(crbug.com/40436245): Remove since BrowserPlugin has been removed.
  void DidAttach();

  // BrowserPluginGuestDelegate implementation.
  std::unique_ptr<content::WebContents> CreateNewGuestWindow(
      const content::WebContents::CreateParams& create_params) final;
  content::WebContents* GetOwnerWebContents() final;
  base::WeakPtr<content::BrowserPluginGuestDelegate> GetGuestDelegateWeakPtr()
      final;

  // WebContentsDelegate implementation.
  void ActivateContents(content::WebContents* contents) final;
  void ContentsMouseEvent(content::WebContents* source,
                          const ui::Event& event) final;
  void ContentsZoomChange(bool zoom_in) final;
  void ResizeDueToAutoResize(content::WebContents* web_contents,
                             const gfx::Size& new_size) final;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) final;
  bool ShouldFocusPageAfterCrash(content::WebContents* source) final;
  void UpdatePreferredSize(content::WebContents* web_contents,
                           const gfx::Size& pref_size) final;
  void UpdateTargetURL(content::WebContents* source, const GURL& url) final;

  // WebContentsObserver implementation.
  void DidStopLoading() final;

  // zoom::ZoomObserver implementation.
  void OnZoomControllerDestroyed(zoom::ZoomController* source) final;
  void OnZoomChanged(
      const zoom::ZoomController::ZoomChangedEventData& data) final;

  void SendQueuedEvents();

  void CompleteInit(base::Value::Dict create_params,
                    GuestCreatedCallback callback,
                    std::unique_ptr<GuestViewBase> owned_this,
                    std::unique_ptr<content::WebContents> guest_web_contents);

  // Dispatches the onResize event to the embedder.
  void DispatchOnResizeEvent(const gfx::Size& old_size,
                             const gfx::Size& new_size);

  // Returns the default size of the guestview.
  gfx::Size GetDefaultSize() const;

  // Get the zoom factor for the embedder's web contents.
  double GetEmbedderZoomFactor() const;

  void SetUpSizing(const base::Value::Dict& params);

  void StartTrackingEmbedderZoomLevel();
  void StopTrackingEmbedderZoomLevel();

  void UpdateGuestSize(const gfx::Size& new_size, bool due_to_auto_resize);

  void SetOwnerHost();

  // This guest tracks the lifetime of the WebContents of `owner_rfh_id_`. If
  // that WebContents is destroyed, then this guest will also self-destruct.
  content::GlobalRenderFrameHostId owner_rfh_id_;
  std::string owner_host_;

  const raw_ptr<content::BrowserContext> browser_context_;

  // |guest_instance_id_| is a profile-wide unique identifier for a guest
  // WebContents.
  const int guest_instance_id_;

  // |view_instance_id_| is an identifier that's unique within a particular
  // embedder RenderViewHost for a particular <*view> instance.
  int view_instance_id_ = kInstanceIDNone;

  // |element_instance_id_| is an identifer that's unique to a particular
  // GuestViewContainer element.
  int element_instance_id_ = kInstanceIDNone;

  // |attach_in_progress_| is used to make sure that attached() doesn't return
  // true until after DidAttach() is called, since that's when we are guaranteed
  // that the contentWindow for cross-process-iframe based guests will become
  // valid.
  bool attach_in_progress_ = false;

  // Indicates that this guest is in the process of being destroyed.
  bool is_being_destroyed_ = false;

  // This is a queue of Events that are destined to be sent to the embedder once
  // the guest is attached to a particular embedder.
  base::circular_deque<std::unique_ptr<GuestViewEvent>> pending_events_;

  // The opener guest view.
  base::WeakPtr<GuestViewBase> opener_;

  // The parameters associated with the element hosting this GuestView that
  // are passed in from JavaScript. This will typically be the view instance ID,
  // and element-specific parameters. These parameters are passed along to new
  // guests that are created from this guest.
  base::Value::Dict attach_params_;

  // This observer ensures that this guest self-destructs if the embedder goes
  // away. It also tracks when the embedder's fullscreen is toggled so the guest
  // can change itself accordingly.
  std::unique_ptr<OwnerContentsObserver> owner_contents_observer_;

  // This observer ensures that if the guest is unattached and its opener goes
  // away then this guest also self-destructs.
  std::unique_ptr<OpenerLifetimeObserver> opener_lifetime_observer_;

  // The size of the guest content. Note: In autosize mode, the container
  // element may not match the size of the guest.
  gfx::Size guest_size_;

  // Before attachment a GuestViewBase is owned with a unique_ptr. After
  // attachment, a GuestViewBase is self-owned and scoped to the lifetime of the
  // guest WebContents.
  bool self_owned_ = false;

  // When the guest is created, a guest WebContents is created and we take
  // ownership of it here until it's ready to be attached. On attachment,
  // ownership of the guest WebContents is taken by the embedding WebContents.
  std::unique_ptr<content::WebContents> owned_guest_contents_;

  // The params used when creating the guest contents. These are saved here in
  // case we need to recreate the guest contents. Not all guest types need to
  // store these.
  std::optional<
      std::pair<base::Value::Dict, content::WebContents::CreateParams>>
      create_params_;

  // Indicates whether autosize mode is enabled or not.
  bool auto_size_enabled_ = false;

  // The maximum size constraints of the container element in autosize mode.
  gfx::Size max_auto_size_;

  // The minimum size constraints of the container element in autosize mode.
  gfx::Size min_auto_size_;

  // The size that will be used when autosize mode is disabled.
  gfx::Size normal_size_;

  // Whether the guest view is inside a plugin document.
  bool is_full_page_plugin_ = false;

  // Used to observe the ZoomControllers of the guest and the embedder.
  base::ScopedMultiSourceObservation<zoom::ZoomController, zoom::ZoomObserver>
      zoom_controller_observations_{this};

  // This is used to ensure pending tasks will not fire after this object is
  // destroyed.
  base::WeakPtrFactory<GuestViewBase> weak_ptr_factory_{this};
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_BASE_H_
