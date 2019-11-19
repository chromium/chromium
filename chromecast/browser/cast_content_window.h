// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_
#define CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/graphics/gestures/cast_gesture_handler.h"
#include "chromecast/ui/mojom/media_control_ui.mojom.h"
#include "chromecast/ui/mojom/ui_service.mojom.h"
#include "ui/events/event.h"

namespace chromecast {

// Describes visual context of the window within the UI.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chromecast.shell
enum class VisibilityType {
  // Unknown visibility state.
  UNKNOWN = 0,

  // Window is occupying the entire screen and can be interacted with.
  FULL_SCREEN = 1,

  // Window occupies a portion of the screen, supporting user interaction.
  PARTIAL_OUT = 2,

  // Window is hidden after dismissal by back gesture, and cannot be interacted
  // with via touch.
  HIDDEN = 3,

  // Window is being displayed as a small visible tile.
  TILE = 4,

  // Window is covered by other activities and cannot be interacted with via
  // touch.
  TRANSIENTLY_HIDDEN = 5
};

// Represents requested activity windowing behavior. Behavior includes:
// 1. How long the activity should show
// 2. Whether the window should become immediately visible
// 3. How much screen space the window should occupy
// 4. What state to return to when the activity is completed
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chromecast.shell
enum class VisibilityPriority {
  // Default priority. It is up to system to decide how to show the activity.
  DEFAULT = 0,

  // The activity wants to occupy the full screen for some period of time and
  // then become hidden after a timeout. When timeout, it returns to the
  // previous activity.
  TRANSIENT_TIMEOUTABLE = 1,

  // A high priority interruption occupies half of the screen if a sticky
  // activity is showing on the screen. Otherwise, it occupies the full screen.
  HIGH_PRIORITY_INTERRUPTION = 2,

  // The activity takes place of other activity and won't be timeout.
  STICKY_ACTIVITY = 3,

  // The activity stays on top of others (transient) but won't be timeout.
  // When the activity finishes, it returns to the previous one.
  TRANSIENT_STICKY = 4,

  // The activity should not be visible.
  HIDDEN = 5,

  // The activity should not be visible, but the activity will consider itself
  // to be visible. This is useful for opaque overlays while the activity is
  // still active.
  HIDDEN_STICKY = 6,
};

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chromecast.shell
enum class GestureType {
  NO_GESTURE = 0,
  GO_BACK = 1,
  TAP = 2,
  TAP_DOWN = 3,
  TOP_DRAG = 4,
  RIGHT_DRAG = 5,
};

// Class that represents the "window" a WebContents is displayed in cast_shell.
// For Linux, this represents an Aura window. For Android, this is a Activity.
// See CastContentWindowAura and CastContentWindowAndroid.
class CastContentWindow {
 public:
  class Delegate {
   public:
    // Notify window destruction.
    virtual void OnWindowDestroyed() {}

    // Check to see if the gesture can be handled by the delegate. This is
    // called prior to ConsumeGesture().
    virtual bool CanHandleGesture(GestureType gesture_type) = 0;

    // Called while a system UI gesture is in progress.
    virtual void GestureProgress(GestureType gesture_type,
                                 const gfx::Point& touch_location) {}

    // Called when an in-progress system UI gesture is cancelled (for example
    // when the finger is lifted before the completion of the gesture.)
    virtual void CancelGesture(GestureType gesture_type,
                               const gfx::Point& touch_location) {}

    // Consume and handle a completed UI gesture. Returns whether the gesture
    // was handled or not.
    virtual bool ConsumeGesture(GestureType gesture_type) = 0;

    // Notify visibility change for this window.
    virtual void OnVisibilityChange(VisibilityType visibility_type) {}

    // Returns app ID of cast activity or application.
    virtual std::string GetId() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // The parameters used to create a CastContentWindow instance.
  struct CreateParams {
    // The delegate for the CastContentWindow. Must be non-null. If the delegate
    // is destroyed before CastContentWindow, the WeakPtr will be invalidated on
    // the main UI thread.
    base::WeakPtr<Delegate> delegate = nullptr;

    // True if this CastContentWindow is for a headless build.
    bool is_headless = false;

    // Enable touch input for this CastContentWindow instance.
    bool enable_touch_input = false;

    // True if this CastContentWindow is for running a remote control app.
    bool is_remote_control_mode = false;

    // True if this app should turn on the screen.
    bool turn_on_screen = true;

    // application or acitivity's session ID
    std::string session_id = "";

    // Gesture priority for when the window is visible.
    CastGestureHandler::Priority gesture_priority =
        CastGestureHandler::Priority::NONE;

    CreateParams();
    CreateParams(const CreateParams& other);
    ~CreateParams();
  };

  class Observer : public base::CheckedObserver {
   public:
    // Notify visibility change for this window.
    virtual void OnVisibilityChange(VisibilityType visibility_type) {}

   protected:
    ~Observer() override {}
  };

  explicit CastContentWindow(const CreateParams& params);
  virtual ~CastContentWindow();

  // Creates a full-screen window for |cast_web_contents| and displays it if
  // screen access has been granted. |cast_web_contents| must outlive the
  // CastContentWindow. |z_order| is provided so that windows which share the
  // same parent have a well-defined order.
  // TODO(seantopping): This method probably shouldn't exist; this class should
  // use RAII instead.
  virtual void CreateWindowForWebContents(
      CastWebContents* cast_web_contents,
      mojom::ZOrder z_order,
      VisibilityPriority visibility_priority) = 0;

  // Allows the window to be shown on the screen. The window cannot be shown on
  // the screen until this is called.
  virtual void GrantScreenAccess() = 0;

  // Prevents the window from being shown on the screen until
  // GrantScreenAccess() is called.
  virtual void RevokeScreenAccess() = 0;

  // Enables touch input to be routed to the window's WebContents.
  virtual void EnableTouchInput(bool enabled) = 0;

  // Cast activity or application calls it to request for a visibility priority
  // change.
  virtual void RequestVisibility(VisibilityPriority visibility_priority) = 0;

  // Provide activity-related metadata. This data should include information
  // that is common for all activities, such as type.
  // TODO(seantopping): Define a schema for this data.
  virtual void SetActivityContext(base::Value activity_context) = 0;

  // Use this to stash custom data for this class. This data will be visible to
  // the window manager.
  virtual void SetHostContext(base::Value host_context) = 0;

  // Notify the window that its visibility type has changed. This should only
  // ever be called by the window manager.
  // TODO(seantopping): Make this private to the window manager.
  virtual void NotifyVisibilityChange(VisibilityType visibility_type) = 0;

  // Cast activity or application calls it to request for moving out of the
  // screen.
  virtual void RequestMoveOut() = 0;

  // Media control interface. Non-null on Aura platforms.
  virtual mojom::MediaControlUi* media_controls();

  // Observer interface:
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  base::WeakPtr<Delegate> delegate_;
  base::ObserverList<Observer> observer_list_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_
