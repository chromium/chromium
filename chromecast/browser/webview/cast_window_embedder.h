// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_CAST_WINDOW_EMBEDDER_H_
#define CHROMECAST_BROWSER_WEBVIEW_CAST_WINDOW_EMBEDDER_H_

#include <string>

#include "chromecast/browser/cast_content_window.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebContents;
}  // namespace content

namespace chromecast {
class CastWebContents;

// An interface to route messages and requests between a window manager
// other than CastShell and the embedded Cast window (i.e. subclass of
// CastContentWindow).
class CastWindowEmbedder {
 public:
  // Describes change of the visibility state of the embedded window.
  enum class VisibilityChange {
    // Unknown visibility state.
    UNKNOWN = 0,

    // The window is not visible to the user.
    NOT_VISIBLE = 1,

    // The window is active and shown fullscreen to the user.
    FULL_SCREEN = 2,

    // The window it is covered by other activities.
    OBSCURED = 3,

    // The window is interrupting another activity, and shown as a side
    // interruption.
    INTERRUPTION = 4,

    // The cast window is interrupted by another activity, and only partially
    // visible.
    INTERRUPTED = 5,
  };

  enum class NavigationType {
    // Unknown nagvigation type.
    UNKNOWN = 0,

    // Navigate back.
    GO_BACK = 1,
  };

  struct BackGestureProgressEvent {
    // The x-coordinate of the finger during the swipe.
    double x = -1;

    // The y-coordinate of the finger during the swipe.
    double y = -1;
  };

  // Event sent from the embedder to instruct corresponding Cast window
  // to respond.
  // Four event types are supported:
  // - Visibility change
  // - Navigation
  // - Back gesture progress update
  // - Cancellation of back gesture
  // Note that a |WindowEvent| must and must only convey one type
  // of event at a time.
  struct EmbedderWindowEvent {
    EmbedderWindowEvent();
    ~EmbedderWindowEvent();

    // Unique window ID assigned by the embedder.
    int window_id = -1;
    absl::optional<VisibilityChange> visibility_changed;
    absl::optional<NavigationType> navigation;
    absl::optional<BackGestureProgressEvent> back_gesture_progress_event;
    absl::optional<bool> back_gesture_cancel_event;
  };

  // Interface for the embedded Cast window to implement for
  // working with the embedder environment.
  class EmbeddedWindow {
   public:
    virtual ~EmbeddedWindow() = default;

    // Returns the unique window ID assigned by the embedder.
    virtual int GetWindowId() = 0;

    // Returns the ID of the hosted app.
    virtual std::string GetAppId() = 0;

    // Handles window change requested by the embedder.
    virtual void OnEmbedderWindowEvent(const EmbedderWindowEvent& request) = 0;

    // Returns the WebContents associsated with this Cast window.
    virtual content::WebContents* GetWebContents() = 0;

    // Returns the CastWebContents associsated with this Cast window.
    virtual CastWebContents* GetCastWebContents() = 0;

    // Populates and reports current window properties to the
    // |CastWindowEmbedder|.
    virtual void DispatchState() = 0;

    // Sends |context| along with current |CastWindowProperties| to
    // the embedder window manager.
    virtual void SendAppContext(const std::string& context) = 0;

    // Stops the Cast window and its assiciated |CastWebContents|.
    virtual void Stop() = 0;
  };

  // Info that are needed by the embedder to orchectrate the embedded window
  // with other activities' window.
  struct CastWindowProperties {
    CastWindowProperties();
    ~CastWindowProperties();
    CastWindowProperties(CastWindowProperties&& other);

    CastWindowProperties(const CastWindowProperties&) = delete;
    CastWindowProperties& operator=(const CastWindowProperties&) = delete;

    // Unique ID for the embedded Cast window. This must be set for each
    // CastWindowProperties.
    int window_id = -1;

    // A Cast specific ID for identifying the type of content hosted by the
    // Cast window.
    std::string app_id;

    // A unique Id to identify the hosted content.
    std::string session_id;

    // Whether the window is a system setup window for OOBE or error screens.
    bool is_system_setup_window = false;

    // Whether the hosted content supports handling touch event.
    bool is_touch_enabled = false;

    // Whether the hosted content is in remote control mode.
    bool is_remote_control = false;

    // Whether the content is enabled/forced to display in 720P resolution.
    bool force_720p_resolution = false;

    // Whether the window support navigate back inside.
    bool supports_go_back_inside = false;

    // Represents requested activity windowing mode.
    VisibilityPriority visibility_priority = VisibilityPriority::DEFAULT;

    // Application-related metadata associated with the Cast window.
    absl::optional<std::string> app_context;

    // Custom data for the Cast window. The embedder and whoever set the
    // value need to have agreement on the schema of |host_context|.
    absl::optional<base::Value> host_context;
  };

  // The embedded Cast window will use this to communicate with the embedder
  // about its current status, including focus change, creation/close of the
  // window, and so forth.
  enum class WindowRequestType {
    // The embedded window requests to report its current set of window
    // properties to the embedder.
    SET_PROPERTIES,

    // Requests the embedder to handle the creation of a new Cast window.
    // This could be called when the embedder is restarted and requests all
    // alive managed Cast window to report its existence.
    OPEN_WINDOW,

    // Informs the embedder that the embedded window is being closed.
    CLOSE_WINDOW,

    // Requests that this window is brought into the focus of user's attention,
    // instructed by its visibility priority setting which is included in
    // |CastWindowProperties|. It generally means window wants to be shown.
    REQUEST_FOCUS,

    // Requests to release the focus of this window, e.g. when the window is
    // hidden and screen access is revoked.
    RELEASE_FOCUS,

    // CastShell has handled GO_BACK navigation request.
    NAVIGATION_HANDLE_RESULT,
  };

  virtual ~CastWindowEmbedder() = default;

  // Generates a new window ID to be used for identifying a unique
  // embedded CastContentWindow.
  virtual int GenerateWindowId() = 0;

  // Add managed |embedded_window| to listen on events that are sent
  // by the embedder.
  virtual void AddEmbeddedWindow(EmbeddedWindow* embedded_window) = 0;

  // Remove |embedded_window| from the list of managed windows.
  virtual void RemoveEmbeddedWindow(EmbeddedWindow* embedded_window) = 0;

  // The embedded window report its updated window properties to the embedder
  // via this function.
  virtual void OnWindowRequest(
      const WindowRequestType& type,
      const CastWindowProperties& window_properties) = 0;

  virtual void GenerateAndSendNavigationHandleResult(
      const int window_id,
      const std::string session_id,
      const bool handled,
      NavigationType navigation_type) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_CAST_WINDOW_EMBEDDER_H_
