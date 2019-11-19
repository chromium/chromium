// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied with modifications from //ash/accessibility, refactored for use in
// chromecast.

#ifndef CHROMECAST_BROWSER_ACCESSIBILITY_TOUCH_EXPLORATION_CONTROLLER_H_
#define CHROMECAST_BROWSER_ACCESSIBILITY_TOUCH_EXPLORATION_CONTROLLER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chromecast/browser/accessibility/accessibility_sound_player.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/events/event.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/gesture_detection/gesture_detector.h"
#include "ui/events/gestures/gesture_provider_aura.h"
#include "ui/gfx/geometry/point.h"

namespace aura {
class Window;
}

namespace ui {
class Event;
class GestureEvent;
class GestureProviderAura;
class TouchEvent;
}  // namespace ui

namespace chromecast {
namespace shell {

// A delegate to handle commands in response to detected accessibility gesture
// events.
class TouchExplorationControllerDelegate {
 public:
  virtual ~TouchExplorationControllerDelegate() {}

  // Called when the user performed an accessibility gesture while in touch
  // accessibility mode, that should be forwarded to ChromeVox.
  virtual void HandleAccessibilityGesture(ax::mojom::Gesture gesture) = 0;

  // Called when the user has performed a single tap, if it is not within
  // lift activation bounds.
  virtual void HandleTap(const gfx::Point touch_location) = 0;
};

// TouchExplorationController is used in tandem with "Spoken Feedback" to
// make the touch UI accessible. Gestures performed in the middle of the screen
// are mapped to accessibility key shortcuts while gestures performed on the
// edge of the screen can change settings.
//
// ** Short version **
//
// At a high-level, single-finger events are used for accessibility -
// exploring the screen gets turned into mouse moves (which can then be
// spoken by an accessibility service running), a single tap while the user
// is in touch exploration or a double-tap simulates a click, and gestures
// can be used to send high-level accessibility commands. For example, a swipe
// right would correspond to the keyboard short cut shift+search+right.
// Swipes with up to four fingers are also mapped to commands. Slide
// gestures performed on the edge of the screen can change settings
// continuously. For example, sliding a finger along the right side of the
// screen will change the volume. When a user double taps and holds with one
// finger, the finger is passed through as if accessibility was turned off. If
// the user taps the screen with two fingers, the user can silence spoken
// feedback if it is playing.
//
// ** Long version **
//
// Here are the details of the implementation:
//
// When the first touch is pressed, a 300 ms grace period timer starts.
//
// If the user keeps their finger down for more than 300 ms and doesn't
// perform a supported accessibility gesture in that time (e.g. swipe right),
// they enter touch exploration mode, and all movements are translated into
// synthesized mouse move events.
//
// Also, if the user moves their single finger outside a certain slop region
// (without performing a gesture), they enter touch exploration mode earlier
// than 300 ms.
//
// If the user taps and releases their finger, after 300 ms from the initial
// touch, a single mouse move is fired.
//
// While in touch exploration mode, the user can perform a single tap
// if the user releases their finger and taps before 300 ms passes.
// This will result in a click on the last successful touch exploration
// location. This allows the user to perform a single tap
// anywhere to activate it. (See more information on simulated clicks
// below.)
//
// The user can perform swipe gestures in one of the four cardinal directions
// which will be interpreted and used to control the UI. All gestures will only
// be registered if the fingers move outside the slop, and all fingers will only
// be registered if they are completed within the grace period. If a single
// finger gesture fails to be completed within the grace period, the state
// changes to touch exploration mode. If a multi finger gesture fails to be
// completed within the grace period, the user must lift all fingers before
// completing any more actions.
//
// The user's initial tap sets the anchor point. Simulated events are
// positioned relative to the anchor point, so that after exploring to find
// an object the user can double-tap anywhere on the screen to activate it.
// The anchor point is also set by ChromeVox every time it highlights an
// object on the screen. During touch exploration this ensures that
// any simulated events go to the center of the most recently highlighted
// object, rather than to the exact tap location (which could have drifted
// off of the object). This also ensures that when the current ChromeVox
// object changes due to a gesture or input focus changing, simulated
// events go to that object and not the last location touched by a finger.
//
// When the user double-taps, this is treated as a discrete gestures, and
// and event is sent to ChromeVox to activate the current object, whatever
// that is. However, when the user double-taps and holds, any event from that
// finger is passed through, allowing the user to drag. These events are
// passed through with a location that's relative to the anchor point.
//
// If any other fingers are added or removed during a passthrough, they are
// ignored. Once the passthrough finger is released, passthrough stops and
// the state is reset to the no fingers down state.
//
// If the user enters touch exploration mode, they can click without lifting
// their touch exploration finger by tapping anywhere else on the screen with
// a second finger, while the touch exploration finger is still pressed.
//
// Once touch exploration mode has been activated, it remains in that mode until
// all fingers have been released.
//
// If the user places a finger on the edge of the screen and moves their finger
// past slop, a slide gesture is performed. The user can then slide one finger
// along an edge of the screen and continuously control a setting. Once the user
// enters this state, the boundaries that define an edge expand so that the user
// can now adjust the setting within a slightly bigger width along the screen.
// If the user exits this area without lifting their finger, they will not be
// able to perform any actions, however if they keep their finger down and
// return to the "hot edge," then they can still adjust the setting. In order to
// perform other touch accessibility movements, the user must lift their finger.
// If additional fingers are added while in this state, the user will transition
// to passthrough.
//
// Currently, only the right edge is mapped to control the volume. Volume
// control along the edge of the screen is directly proportional to where the
// user's finger is located on the screen. The top right corner of the screen
// automatically sets the volume to 100% and the bottome right corner of the
// screen automatically sets the volume to 0% once the user has moved past slop.
//
// If the user taps the screen with two fingers and lifts both fingers before
// the grace period has passed, spoken feedback is silenced.
//
// The user can also enter passthrough by placing a finger on one of the bottom
// corners of the screen until an earcon sounds. After the earcon sounds, the
// user is in passthrough so all subsequent fingers placed on the screen will be
// passed through. Once the finger in the corner has been released, the state
// will switch to wait for no fingers.
//
// The caller is expected to retain ownership of instances of this class and
// destroy them before |root_window| is destroyed.
class TouchExplorationController : public ui::EventRewriter,
                                   public ui::GestureProviderAuraClient,
                                   public ui::GestureConsumer {
 public:
  TouchExplorationController(
      aura::Window* root_window,
      TouchExplorationControllerDelegate* delegate,
      AccessibilitySoundPlayer* accessibility_sound_player);
  ~TouchExplorationController() override;

  // Make synthesized touch events are anchored at this point. This is
  // called when the object with accessibility focus is updated via something
  // other than touch exploration.
  void SetTouchAccessibilityAnchorPoint(const gfx::Point& anchor_point);

  // Events within the exclude bounds will not be rewritten.
  // |bounds| are in root window coordinates.
  void SetExcludeBounds(const gfx::Rect& bounds);

  // Overridden from ui::EventRewriter
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

 private:
  friend class TouchExplorationControllerTestApi;

  // Event handlers based on the current state - see State, below.
  ui::EventDispatchDetails InNoFingersDown(const ui::TouchEvent& event,
                                           const Continuation continuation);
  ui::EventDispatchDetails InSingleTapPressed(const ui::TouchEvent& event,
                                              const Continuation continuation);
  ui::EventDispatchDetails InSingleTapOrTouchExploreReleased(
      const ui::TouchEvent& event,
      const Continuation continuation);
  ui::EventDispatchDetails InDoubleTapPending(const ui::TouchEvent& event,
                                              const Continuation continuation);
  ui::EventDispatchDetails InTouchReleasePending(
      const ui::TouchEvent& event,
      const Continuation continuation);
  ui::EventDispatchDetails InTouchExploration(const ui::TouchEvent& event,
                                              const Continuation continuation);
  ui::EventDispatchDetails InOneFingerPassthrough(
      const ui::TouchEvent& event,
      const Continuation continuation);
  ui::EventDispatchDetails InGestureInProgress(const ui::TouchEvent& event,
                                               const Continuation continuation);
  ui::EventDispatchDetails InTouchExploreSecondPress(
      const ui::TouchEvent& event,
      const Continuation continuation);
  ui::EventDispatchDetails InWaitForNoFingers(const ui::TouchEvent& event,
                                              const Continuation continuation);
  ui::EventDispatchDetails InTwoFingerTap(const ui::TouchEvent& event,
                                          const Continuation continuation);

  // Returns the current time of the tick clock.
  base::TimeTicks Now();

  // This timer is started every time we get the first press event, and
  // it fires after the double-click timeout elapses (300 ms by default).
  // If the user taps and releases within 300 ms and doesn't press again,
  // we treat that as a single mouse move (touch exploration) event.
  void StartTapTimer();
  void OnTapTimerFired();

  // Dispatch a new event outside of the event rewriting flow.
  void DispatchEvent(ui::Event* event, const Continuation continuation);

  // Overridden from GestureProviderAuraClient.
  //
  // The gesture provider keeps track of all the touch events after
  // the user moves fast enough to trigger a gesture. After the user
  // completes their gesture, this method will decide what keyboard
  // input their gesture corresponded to.
  void OnGestureEvent(ui::GestureConsumer* raw_input_consumer,
                      ui::GestureEvent* gesture) override;

  // Process the gesture events that have been created.
  void ProcessGestureEvents();

  void OnSwipeEvent(ui::GestureEvent* swipe_gesture);

  // Dispatches a single key with the given flags.
  void DispatchKeyWithFlags(const ui::KeyboardCode key,
                            int flags,
                            const Continuation continuation);

  // Binds DispatchKeyWithFlags to a specific key and flags.
  base::OnceClosure BindKeyEventWithFlags(const ui::KeyboardCode key,
                                          int flags,
                                          const Continuation continuation);

  std::unique_ptr<ui::MouseEvent> CreateMouseMoveEvent(
      const gfx::PointF& location,
      int flags);

  void EnterTouchToMouseMode();

  void PlaySoundForTimer();

  // Sends a simulated click, if an anchor point was set explicitly. Otherwise,
  // sends a simulated tap at anchor point.
  void SendSimulatedClickOrTap(const Continuation continuation);

  // Sends a simulated tap at anchor point.
  void SendSimulatedTap(const Continuation continuation);

  // Sends a simulated tap, if the anchor point falls within lift activation
  // bounds.
  void MaybeSendSimulatedTapInLiftActivationBounds(
      const ui::TouchEvent& event,
      const Continuation continuation);

  // Some constants used in touch_exploration_controller:

  // Within this many dips of the screen edge, the release event generated will
  // reset the state to NoFingersDown.
  const float kLeavingScreenEdge = 35;

  // Touch within this distance from a corner can invoke corner passthrough.
  const float kMaxDistanceFromEdge = 75;

  // After a slide gesture has been triggered, if the finger is still within
  // these bounds (in DIPs), the preset settings will still change.
  const float kSlopDistanceFromEdge = kMaxDistanceFromEdge + 40;

  // The split tap slop  is a bit more generous since keeping two
  // fingers in place is a bit harder.
  float GetSplitTapTouchSlop();

  // Convert a gfx::PointF from DIP back to raw screen coordinates.
  gfx::PointF ConvertDIPToScreenInPixels(const gfx::PointF& location);

  enum State {
    // No fingers are down and no events are pending.
    NO_FINGERS_DOWN,

    // A single finger is down, but we're not yet sure if this is going
    // to be touch exploration or something else.
    SINGLE_TAP_PRESSED,

    // The user pressed and released a single finger - a tap - but we have
    // to wait until the end of the grace period to allow the user to tap the
    // second time. If the second tap doesn't occurs within the grace period,
    // we dispatch a mouse move at the location of the first tap.
    SINGLE_TAP_RELEASED,

    // The user was in touch explore mode and released the finger.
    // If another touch press occurs within the grace period, a single
    // tap click occurs. This state differs from SINGLE_TAP_RELEASED
    // in that if a second tap doesn't occur within the grace period,
    // there is no mouse move dispatched.
    TOUCH_EXPLORE_RELEASED,

    // The user tapped once, and before the grace period expired, pressed
    // one finger down to begin a double-tap, but has not released it yet.
    // This could become passthrough, so no touch press is dispatched yet.
    DOUBLE_TAP_PENDING,

    // The user was doing touch exploration, started split tap, but lifted the
    // touch exploration finger. Once they remove all fingers, a touch release
    // will go through.
    TOUCH_RELEASE_PENDING,

    // We're in touch exploration mode. Anything other than the first finger
    // is ignored, and movements of the first finger are rewritten as mouse
    // move events. This mode is entered if a single finger is pressed and
    // after the grace period the user hasn't added a second finger or
    // moved the finger outside of the slop region. We'll stay in this
    // mode until all fingers are lifted.
    TOUCH_EXPLORATION,

    // If the user moves their finger faster than the threshold velocity after a
    // single tap, the touch events that follow will be translated into gesture
    // events. If the user successfully completes a gesture within the grace
    // period, the gesture will be interpreted and used to control the UI via
    // discrete actions - currently by synthesizing key events corresponding to
    // each gesture Otherwise, the collected gestures are discarded and the
    // state changes to touch_exploration.
    GESTURE_IN_PROGRESS,

    // The user was in touch exploration, but has placed down another finger.
    // If the user releases the second finger, a touch press and release
    // will go through at the last touch explore location. If the user
    // releases the touch explore finger, the touch press and release will
    // still go through once the split tap finger is also lifted. If any
    // fingers pressed past the first two, the touch press is cancelled and
    // the user enters the wait state for the fingers to be removed.
    TOUCH_EXPLORE_SECOND_PRESS,

    // After the user double taps and holds with a single finger, all events
    // for that finger are passed through, displaced by an offset. Adding
    // extra fingers has no effect. This state is left when the user removes
    // all fingers.
    ONE_FINGER_PASSTHROUGH,

    // If the user added another finger in SINGLE_TAP_PRESSED, or if the user
    // has multiple fingers fingers down in any other state between
    // passthrough, touch exploration, and gestures, they must release
    // all fingers before completing any more actions. This state is
    // generally useful for developing new features, because it creates a
    // simple way to handle a dead end in user flow.
    WAIT_FOR_NO_FINGERS,

    // If the user taps the screen with two fingers and releases both fingers
    // before the grace period has passed, spoken feedback will be silenced.
    TWO_FINGER_TAP,
  };

  enum AnchorPointState {
    ANCHOR_POINT_NONE,
    ANCHOR_POINT_FROM_TOUCH_EXPLORATION,
    ANCHOR_POINT_EXPLICITLY_SET
  };

  enum ScreenLocation {
    // Hot "edges" of the screen are each represented by a respective bit.
    NO_EDGE = 0,
    RIGHT_EDGE = 1 << 0,
    TOP_EDGE = 1 << 1,
    LEFT_EDGE = 1 << 2,
    BOTTOM_EDGE = 1 << 3,
  };

  // Given a point, if it is within the given inset of an edge, returns the
  // edge. If it is within the given inset of two edges, returns an int with
  // both bits that represent the respective edges turned on. Otherwise returns
  // SCREEN_CENTER.
  int FindEdgesWithinInset(gfx::Point point,
                           float horiz_inset,
                           float vert_inset);

  // Set the state and modifies any variables related to the state change.
  // (e.g. resetting the gesture provider).
  void SetState(State new_state, const char* function_name);

  void VlogState(const char* function_name);

  void VlogEvent(const ui::TouchEvent& event, const char* function_name);

  // Gets enum name from integer value.
  const char* EnumStateToString(State state);

  aura::Window* root_window_;

  // Handles accessibility gestures. Not owned.
  TouchExplorationControllerDelegate* delegate_;

  // Handles earcons. Not owned.
  AccessibilitySoundPlayer* accessibility_sound_player_;

  // A set of touch ids for fingers currently touching the screen.
  std::vector<int> current_touch_ids_;

  // Map of touch ids to their last known location.
  std::map<int, gfx::PointF> touch_locations_;

  // The current state.
  State state_;

  // A copy of the event from the initial touch press.
  std::unique_ptr<ui::TouchEvent> initial_press_;
  Continuation initial_press_continuation_;

  // The timestamp of the most recent press event for the main touch id.
  // The difference between this and |initial_press_->time_stamp| is that
  // |most_recent_press_timestamp_| is reset in a double-tap.
  base::TimeTicks most_recent_press_timestamp_;

  // Map of touch ids to where its initial press occurred relative to the
  // screen.
  std::map<int, gfx::Point> initial_presses_;

  // In one finger passthrough, the touch is displaced relative to the
  // last touch exploration location.
  gfx::Vector2dF passthrough_offset_;

  // Stores the most recent event from a finger that is currently not
  // sending events through, but might in the future (e.g. before a finger
  // enters double-tap-hold passthrough, we need to update its location.)
  std::unique_ptr<ui::TouchEvent> last_unused_finger_event_;
  Continuation last_unused_finger_continuation_;

  // The anchor point used as the location of a synthesized tap when the
  // user double-taps anywhere on the screen, and similarly the initial
  // point used when the user double-taps, holds, and drags. This can be
  // set either via touch exploration, or by a call to
  // SetTouchAccessibilityAnchorPoint when focus moves due to something other
  // than touch exploration.
  gfx::PointF anchor_point_dip_;

  // The current state of the anchor point.
  AnchorPointState anchor_point_state_;

  // The last touch exploration event.
  std::unique_ptr<ui::TouchEvent> last_touch_exploration_;

  // A timer that fires after the double-tap delay.
  base::OneShotTimer tap_timer_;

  // A timer to fire an indicating sound when sliding to change volume.
  base::RepeatingTimer sound_timer_;

  // A default gesture detector config, so we can share the same
  // timeout and pixel slop constants.
  ui::GestureDetector::Config gesture_detector_config_;

  // Gesture Handler to interpret the touch events.
  std::unique_ptr<ui::GestureProviderAura> gesture_provider_;

  // The previous state entered.
  State prev_state_;

  // A copy of the previous event passed.
  std::unique_ptr<ui::TouchEvent> prev_event_;

  // This toggles whether DVLOGS are turned on or not.
  bool DVLOG_on_;

  // LocatedEvents within this area should be left alone.
  gfx::Rect exclude_bounds_;

  // Any touch exploration that both starts and ends (touch pressed, and
  // released) within this rectangle, triggers a simulated single finger tap at
  // the anchor point on release.
  gfx::Rect lift_activation_bounds_;

  // Whether or not we've seen a touch press event yet.
  bool seen_press_ = false;

  // The maximum touch points seen in the current gesture.
  size_t max_gesture_touch_points_ = 0;

  // The horizontal and vertical insets for side gesture detection.
  const int gesture_start_width_;
  const int gesture_start_height_;

  // Whether or not to trigger pass through mode when touch events come
  // in from the edges.
  bool side_gesture_pass_through_;

  DISALLOW_COPY_AND_ASSIGN(TouchExplorationController);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_ACCESSIBILITY_TOUCH_EXPLORATION_CONTROLLER_H_
