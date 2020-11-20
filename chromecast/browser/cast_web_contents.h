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
#include "base/optional.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "chromecast/common/mojom/feature_manager.mojom.h"
#include "content/public/common/media_playback_renderer_type.mojom.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/messaging/web_message_port.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace blink {
class AssociatedInterfaceProvider;
}  // namespace blink

namespace content {
class WebContents;
}  // namespace content

namespace on_load_script_injector {
template <typename>
class OnLoadScriptInjectorHost;
}  // namespace on_load_script_injector

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
// the main frame is fully loaded and running (all resources fetched, JS is
// running). Iframes might still be loading in this case, but in general we
// consider the page to be in a presentable state at this stage, so it is
// appropriate to display the WebContents to the user.
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
class CastWebContents {
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

  // Observer class. The Observer should *not* destroy CastWebContents during
  // any of these events, otherwise other observers might try to use a freed
  // pointer to |cast_web_contents|.
  class Observer {
   public:
    Observer();

    // Advertises page state for the CastWebContents.
    // Use CastWebContents::page_state() to get the new state.
    virtual void OnPageStateChanged(CastWebContents* cast_web_contents) {}

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
    virtual void OnPageStopped(CastWebContents* cast_web_contents,
                               int error_code) {}

    // A new RenderFrame was created for the WebContents. |frame_interfaces| are
    // provided by the new frame.
    virtual void RenderFrameCreated(
        int render_process_id,
        int render_frame_id,
        service_manager::InterfaceProvider* frame_interfaces,
        blink::AssociatedInterfaceProvider* frame_associated_interfaces) {}

    // A navigation has finished in the WebContents' main frame.
    virtual void MainFrameFinishedNavigation() {}

    // These methods are calls forwarded from WebContentsObserver.
    virtual void MainFrameResized(const gfx::Rect& bounds) {}
    virtual void UpdateTitle(const base::string16& title) {}
    virtual void UpdateFaviconURL(GURL icon_url) {}
    virtual void DidFinishBlockedNavigation(GURL url) {}
    virtual void DidFirstVisuallyNonEmptyPaint() {}

    // Notifies that a resource for the main frame failed to load.
    virtual void ResourceLoadFailed(CastWebContents* cast_web_contents) {}

    // Propagates the process information via observer, in particular to
    // the underlying OnRendererProcessStarted() method.
    virtual void OnRenderProcessReady(const base::Process& process) {}

    // Notify media playback state changes for the underlying WebContents.
    virtual void MediaPlaybackChanged(bool media_playing) {}

    // Adds |this| to the ObserverList in the implementation of
    // |cast_web_contents|.
    void Observe(CastWebContents* cast_web_contents);

    // Removes |this| from the ObserverList in the implementation of
    // |cast_web_contents_|. This is only invoked by CastWebContents and is used
    // to ensure that once the observed CastWebContents object is destructed the
    // CastWebContents::Observer does not invoke any additional function calls
    // on it.
    void ResetCastWebContents();

   protected:
    virtual ~Observer();

    CastWebContents* cast_web_contents_;
  };

  enum class BackgroundColor {
    NONE,
    WHITE,
    BLACK,
    TRANSPARENT,
  };

  // Initialization parameters for CastWebContents.
  struct InitParams {
    // The delegate for the CastWebContents. Must be non-null. If the delegate
    // is destroyed before CastWebContents, the WeakPtr will be invalidated on
    // the main UI thread.
    base::WeakPtr<Delegate> delegate = nullptr;
    // Enable development mode for this CastWebContents. Whitelists
    // certain functionality for the WebContents, like remote debugging and
    // debugging interfaces.
    bool enabled_for_dev = false;
    // Chooses a media renderer for the WebContents.
    content::mojom::RendererType renderer_type =
        content::mojom::RendererType::DEFAULT_RENDERER;
    // Whether the WebContents is a root native window, or if it is embedded in
    // another WebContents (see Delegate::InnerContentsCreated()).
    bool is_root_window = false;
    // Whether inner WebContents events should be handled. If this is set to
    // true, then inner WebContents will automatically have a CastWebContents
    // created and notify the delegate.
    bool handle_inner_contents = false;
    // Construct internal media blocker and enable BlockMediaLoading().
    bool use_media_blocker = false;
    // Background color for the WebContents view. If not provided, the color
    // will fall back to the platform default.
    BackgroundColor background_color = BackgroundColor::NONE;
    // Enable WebSQL database for this CastWebContents.
    bool enable_websql = false;
    // Enable mixer audio support for this CastWebContents.
    bool enable_mixer_audio = false;
    // Whether to provide a URL filter applied to network requests for the
    // activity hosted by this CastWebContents.
    // No filters implies no restrictions.
    base::Optional<std::vector<std::string>> url_filters = base::nullopt;

    InitParams();
    InitParams(const InitParams& other);
    ~InitParams();
  };

  // Page state for the main frame.
  enum class PageState {
    IDLE,       // Main frame has not started yet.
    LOADING,    // Main frame is loading resources.
    LOADED,     // Main frame is loaded, but sub-frames may still be loading.
    CLOSED,     // Page is closed and should be cleaned up.
    DESTROYED,  // The WebContents is destroyed and can no longer be used.
    ERROR,      // Main frame is in an error state.
  };

  static std::vector<CastWebContents*>& GetAll();

  // Returns the CastWebContents that wraps the content::WebContents, or nullptr
  // if the CastWebContents does not exist.
  static CastWebContents* FromWebContents(content::WebContents* web_contents);

  CastWebContents() = default;
  virtual ~CastWebContents() = default;

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
  virtual base::Optional<pid_t> GetMainFrameRenderProcessPid() const = 0;

  // ===========================================================================
  // Initialization and Setup
  // ===========================================================================

  // Add a set of features for all renderers in the WebContents. Features are
  // configured when `CastWebContents::RenderFrameCreated` is invoked.
  virtual void AddRendererFeatures(std::vector<RendererFeature> features) = 0;

  virtual void AllowWebAndMojoWebUiBindings() = 0;
  virtual void ClearRenderWidgetHostView() = 0;

  // ===========================================================================
  // Page Lifetime
  // ===========================================================================

  // Navigates the underlying WebContents to |url|. Delegate will be notified of
  // page progression events via OnPageStateChanged().
  virtual void LoadUrl(const GURL& url) = 0;

  // Initiate closure of the page. This invokes the appropriate unload handlers.
  // Eventually the delegate will be notified with OnPageStopped().
  virtual void ClosePage() = 0;

  // Stop the page immediately. This will automatically invoke
  // Delegate::OnPageStopped(error_code), allowing the delegate to delete or
  // reload the page without waiting for the WebContents owner to tear down the
  // page.
  virtual void Stop(int error_code) = 0;

  // ===========================================================================
  // Visibility
  // ===========================================================================

  // Specify if the WebContents should be treated as visible. This triggers a
  // document "visibilitychange" change event, and will paint the WebContents
  // quad if |visible| is true (otherwise it will be blank). Note that this does
  // *not* guarantee the page is visible on the screen, as that depends on if
  // the WebContents quad is present in the screen layout and isn't obscured by
  // another window.
  virtual void SetWebVisibilityAndPaint(bool visible) = 0;

  // ===========================================================================
  // Media Management
  // ===========================================================================

  // Block/unblock media from loading in all RenderFrames for the WebContents.
  virtual void BlockMediaLoading(bool blocked) = 0;
  // Block/unblock media from starting in all RenderFrames for the WebContents.
  // As opposed to |BlockMediaLoading|,  |BlockMediaStarting| allows media to
  // load while in blocking state.
  virtual void BlockMediaStarting(bool blocked) = 0;
  virtual void EnableBackgroundVideoPlayback(bool enabled) = 0;

  // ===========================================================================
  // Page Communication
  // ===========================================================================

  // Returns the script injector instance, which injects scripts at page load
  // time.
  virtual on_load_script_injector::OnLoadScriptInjectorHost<std::string>*
  script_injector() = 0;

  // Injects on-load scripts into the WebContents' main frame.
  virtual void InjectScriptsIntoMainFrame() = 0;

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
      const base::string16& javascript,
      base::OnceCallback<void(base::Value)> callback) = 0;

  // ===========================================================================
  // Utility Methods
  // ===========================================================================

  // Used to add or remove |observer| to the ObserverList in the implementation.
  // These functions should only be invoked by CastWebContents::Observer in a
  // valid sequence, enforced via SequenceChecker.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Enable or disable devtools remote debugging for this WebContents and any
  // inner WebContents that are spawned from it.
  virtual void SetEnabledForRemoteDebugging(bool enabled) = 0;

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

 private:
  DISALLOW_COPY_AND_ASSIGN(CastWebContents);
};

std::ostream& operator<<(std::ostream& os, CastWebContents::PageState state);

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_CONTENTS_H_
