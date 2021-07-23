// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_CONTENTS_H_
#define CHROMECAST_BROWSER_CAST_WEB_CONTENTS_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "chromecast/bindings/public/mojom/api_bindings.mojom.h"
#include "chromecast/browser/mojom/cast_web_contents.mojom.h"
#include "chromecast/browser/web_types.h"
#include "chromecast/common/mojom/feature_manager.mojom.h"
#include "content/public/common/media_playback_renderer_type.mojom.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/messaging/web_message_port.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace blink {
class AssociatedInterfaceProvider;
}  // namespace blink

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace chromecast {

struct RendererFeature {
  const std::string name;
  base::Value value;
};

// Simplified WebContents wrapper class for Cast platforms.
//
// Proper usage of content::WebContents relies on understanding the meaning
// behind various WebContentsObserver methods, and then translating those
// signals into some concrete state. CastWebContents does *not* own the
// underlying WebContents (usually whatever class implements
// content::WebContentsDelegate is the actual owner).
//
// =============================================================================
// Lifetime
// =============================================================================
// CastWebContents *must* be created before WebContents begins loading any
// content. Once content begins loading (via CWC::LoadUrl() or one of the
// WebContents navigation APIs), CastWebContents will calculate its state based
// on the status of the WebContents' *main* RenderFrame. Events from sub-frames
// (e.g. iframes) are ignored, since we expect the web app to take care of
// sub-frame errors.
//
// We consider the CastWebContents to be in a LOADED state when the content of
// the main frame is fully loaded and running (all resources fetched,
// redirection finished, JS is running). Iframes might still be loading in this
// case, but in general we consider the page to be in a presentable state at
// this stage, so it is appropriate to display the WebContents to the user.
//
// During or after the page is loaded, there are multiple error conditions that
// can occur. The following events will cause the page to enter an ERROR state:
//
// 1. If the main frame is served an HTTP error page (such as a 404 page), then
//    it means the desired content wasn't loaded.
//
// 2. If the main frame fails to load, such as when the browser blocked the URL
//    request, we treat this as an error.
//
// 3. The RenderProcess for the main frame could crash, so the page is not in a
//    usable state.
//
// The CastWebContents user can respond to these errors in a few ways: The
// content can be reloaded, or the entire page activity can be cancelled. If we
// totally cancel the activity, we prefer to notify the user with an error
// screen or visible/audible error message. Otherwise, a silent retry is
// preferred.
//
// CastWebContents can be used to close the underlying WebContents gracefully
// via CWC::Close(). This initiates web page tear-down logic so that the web
// app has a chance to perform its own finalization logic in JS. Next, we call
// WebContents::ClosePage(), which defers the page closure logic to the
// content::WebContentsDelegate. Usually, it will run its own finalization
// logic and then destroy the WebContents. CastWebContents will be notified of
// the WebContents destruction and enter the DESTROYED state. In the event
// the page isn't destroyed, the page will enter the CLOSED state automatically
// after a timeout. CastWebContents users should not try to reload the page, as
// page closure is intentional.
//
// The web app may decide to close itself (such as via "window.close()" in JS).
// This is similar to initiating the close flow via CWC::Close(), with the end
// result being the same. We consider this an intentional closure, and should
// not attempt to reload the page.
//
// Once CastWebContents is in the DESTROYED state, it is not really usable
// anymore; most of the methods will simply no-op, and no more observer signals
// will be emitted.
//
// CastWebContents can be deleted at any time, *except* during Observer
// notifications. If the owner wants to destroy CastWebContents as a result of
// an Observer event, it should post a task to destroy CastWebContents.
class CastWebContents : public mojom::CastWebContents {
 public:
  class Delegate {
   public:
    // Notify that an inner WebContents was created. |inner_contents| is created
    // in a default-initialized state with no delegate, and can be safely
    // initialized by the delegate.
    virtual void InnerContentsCreated(CastWebContents* inner_contents,
                                      CastWebContents* outer_contents) {}

   protected:
    virtual ~Delegate() {}
  };

  // Observers must use Observer::Observe(cast_web_contents | nullptr) to
  // add/remove themselves from the observer list. The Observer must not destroy
  // CastWebContents during any of these events, otherwise other observers might
  // try to use a freed pointer to |cast_web_contents|.
  class Observer : public mojom::CastWebContentsObserver {
   public:
    Observer();

    // Adds |this| to the CastWebContents observer list. Observe(nullptr) will
    // remove |this| from the observer list of the current CastWebContents being
    // observed.
    void Observe(CastWebContents* cast_web_contents);

    // =========================================================================
    // Observer Methods
    // =========================================================================

    // Advertises page state for the CastWebContents.
    void PageStateChanged(PageState page_state) override {}

    // Called when the page has stopped. e.g.: A 404 occurred when loading the
    // page or if the render process for the main frame crashes. |error_code|
    // will return a net::Error describing the failure, or net::OK if the page
    // closed intentionally.
    //
    // After this method, the page state will be one of the following:
    // CLOSED: Page was closed as expected and the WebContents exists. The page
    //     should generally not be reloaded, since the page closure was
    //     triggered intentionally.
    // ERROR: Page is in an error state. It should be reloaded or deleted.
    // DESTROYED: Page was closed due to deletion of WebContents. The
    //     CastWebContents instance is no longer usable and should be deleted.
    void PageStopped(PageState page_state, int error_code) override {}

    // A new RenderFrame was created for the WebContents. |frame_interfaces| are
    // provided by the new frame.
    virtual void RenderFrameCreated(
        int render_process_id,
        int render_frame_id,
        service_manager::InterfaceProvider* frame_interfaces,
        blink::AssociatedInterfaceProvider* frame_associated_interfaces) {}

    // Called when the navigation is ready to be committed in the WebContents'
    // main frame.
    virtual void MainFrameReadyToCommitNavigation(
        content::NavigationHandle* navigation_handle) {}

    // A navigation has finished in the WebContents' main frame.
    void MainFrameFinishedNavigation() override {}

    // These methods are calls forwarded from WebContentsObserver.
    void UpdateTitle(const std::string& title) override {}
    void UpdateFaviconURL(const GURL& url) override {}
    void DidFirstVisuallyNonEmptyPaint() override {}
    virtual void MainFrameResized(const gfx::Rect& bounds) {}

    // Notifies that a resource for the main frame failed to load.
    virtual void ResourceLoadFailed(CastWebContents* cast_web_contents) {}

    // Propagates the process information via observer, in particular to
    // the underlying OnRendererProcessStarted() method.
    void OnRenderProcessReady(int pid) override {}

    // Notify media playback state changes for the underlying WebContents.
    void MediaPlaybackChanged(bool media_playing) override {}

    // Removes |this| from the ObserverList in the implementation of
    // |cast_web_contents_|. This is only invoked by CastWebContents and is used
    // to ensure that once the observed CastWebContents object is destructed the
    // CastWebContents::Observer does not invoke any additional function calls
    // on it.
    void ResetCastWebContents();

   protected:
    ~Observer() override;

    CastWebContents* cast_web_contents_;
    mojo::Receiver<mojom::CastWebContentsObserver> receiver_{this};
  };

  static std::vector<CastWebContents*>& GetAll();

  // Returns the CastWebContents that wraps the content::WebContents, or nullptr
  // if the CastWebContents does not exist.
  static CastWebContents* FromWebContents(content::WebContents* web_contents);

  CastWebContents();
  ~CastWebContents() override;

  // Tab identifier for the WebContents, mainly used by the tabs extension API.
  // Tab IDs may be re-used, but no two live CastWebContents should have the
  // same tab ID at any given time.
  virtual int tab_id() const = 0;

  // An identifier for the WebContents, mainly used by platform views service.
  // IDs may be re-used but are unique among all live CastWebContents.
  virtual int id() const = 0;

  // TODO(seantopping): Hide this, clients shouldn't use WebContents directly.
  virtual content::WebContents* web_contents() const = 0;
  virtual PageState page_state() const = 0;

  // Returns the PID of the main frame process if valid.
  virtual absl::optional<pid_t> GetMainFrameRenderProcessPid() const = 0;

  // mojom::CastWebContents implementation:
  void LoadUrl(const GURL& url) override = 0;
  void ClosePage() override = 0;
  void SetWebVisibilityAndPaint(bool visible) override = 0;
  void BlockMediaLoading(bool blocked) override = 0;
  void BlockMediaStarting(bool blocked) override = 0;
  void EnableBackgroundVideoPlayback(bool enabled) override = 0;
  void SetEnabledForRemoteDebugging(bool enabled) override = 0;
  void AddObserver(
      mojo::PendingRemote<mojom::CastWebContentsObserver> observer) override;

  // ===========================================================================
  // Initialization and Setup
  // ===========================================================================

  // Add a set of features for all renderers in the WebContents. Features are
  // configured when `CastWebContents::RenderFrameCreated` is invoked.
  virtual void AddRendererFeatures(std::vector<RendererFeature> features) = 0;

  // TODO(b/149041392): This can be an initialization parameter.
  virtual void AllowWebAndMojoWebUiBindings() = 0;
  virtual void ClearRenderWidgetHostView() = 0;

  // Associates transparent app properties to a given session ID. This data is
  // used elsewhere in the browser to gate output stream selection. We expose
  // this API on CastWebContents for the sake of convenience.
  virtual void SetAppProperties(const std::string& session_id,
                                bool is_audio_app) = 0;

  // TODO(b/191718807) need to pass App's page permissions here.
  virtual void SetCastPermissionUserData(const std::string& app_id,
                                         const GURL& app_web_url) = 0;

  // ===========================================================================
  // Page Lifetime
  // ===========================================================================

  // Stop the page immediately. This will automatically invoke
  // Delegate::OnPageStopped(error_code), allowing the delegate to delete or
  // reload the page without waiting for the WebContents owner to tear down the
  // page.
  virtual void Stop(int error_code) = 0;

  // ===========================================================================
  // Page Communication
  // ===========================================================================

  // Executes a UTF-8 encoded |script| for every subsequent page load where
  // the frame's URL has an origin reflected in |origins|. The script is
  // executed early, prior to the execution of the document's scripts.
  //
  // Scripts are identified by a client-managed |id|. Any
  // script previously injected using the same |id| will be replaced.
  //
  // The order in which multiple bindings are executed is the same as the
  // order in which the bindings were added. If a script is added which
  // clobbers an existing script of the same |id|, the previous script's
  // precedence in the injection order will be preserved.
  // |script| and |id| must be non-empty string.
  virtual void AddBeforeLoadJavaScript(uint64_t id,
                                       base::StringPiece script) = 0;

  // Posts a message to the frame's onMessage handler.
  //
  // `target_origin` restricts message delivery to the specified origin.
  // If `target_origin` is "*", then the message will be sent to the
  // document regardless of its origin.
  // See html.spec.whatwg.org/multipage/web-messaging.html sect. 9.4.3
  // for more details on how the target origin policy is applied.
  // Should be called on UI thread.
  virtual void PostMessageToMainFrame(
      const std::string& target_origin,
      const std::string& data,
      std::vector<blink::WebMessagePort> ports) = 0;

  // Executes a string of JavaScript in the main frame's context.
  // This is no-op if the main frame is not available.
  // Pass in a callback to receive a result when it is available.
  // If there is no need to receive the result, pass in a
  // default-constructed callback. If provided, the callback
  // will be invoked on the UI thread.
  virtual void ExecuteJavaScript(
      const std::u16string& javascript,
      base::OnceCallback<void(base::Value)> callback) = 0;

  // Connects and fetches JS API bindings from |api_bindings_remote|.
  // This method will fetch bindings scripts from |api_bindings_remote|
  // immediately after the invocation, all of the bindings should be
  // initialized before this point.
  virtual void ConnectToBindingsService(
      mojo::PendingRemote<mojom::ApiBindings> api_bindings_remote) = 0;

  // ===========================================================================
  // Utility Methods
  // ===========================================================================

  // Used to expose CastWebContents's |binder_registry_| to Delegate.
  // Delegate should register its mojo interface binders via this function
  // when it is ready.
  virtual service_manager::BinderRegistry* binder_registry() = 0;

  // Asks the CastWebContents to bind an interface receiver using either its
  // registry or any registered InterfaceProvider.
  virtual bool TryBindReceiver(mojo::GenericPendingReceiver& receiver) = 0;

  // Used for owner to pass its |InterfaceProvider| pointers to CastWebContents.
  // It is owner's responsibility to make sure each |InterfaceProvider| pointer
  // has distinct mojo interface set.
  using InterfaceSet = base::flat_set<std::string>;
  virtual void RegisterInterfaceProvider(
      const InterfaceSet& interface_set,
      service_manager::InterfaceProvider* interface_provider) = 0;

  // Returns true if WebSQL database is configured enabled for this
  // CastWebContents.
  virtual bool is_websql_enabled() = 0;

  // Returns true if mixer audio is enabled.
  virtual bool is_mixer_audio_enabled() = 0;

  // Returns whether or not CastWebContents binder_registry() is valid for
  // binding interfaces.
  virtual bool can_bind_interfaces() = 0;

  // Binds a receiver for remote control of CastWebContents.
  void BindReceiver(mojo::PendingReceiver<mojom::CastWebContents> receiver);

 protected:
  mojo::Receiver<mojom::CastWebContents> receiver_{this};
  mojo::RemoteSet<mojom::CastWebContentsObserver> observers_;

 private:
  friend class Observer;

  // These functions should only be invoked by CastWebContents::Observer in a
  // valid sequence, enforced via SequenceChecker.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  DISALLOW_COPY_AND_ASSIGN(CastWebContents);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_CONTENTS_H_
