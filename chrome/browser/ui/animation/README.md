# Browser Animation Controller

This is a browser-window-level service which coordinates multi-View and
multi-part animations, such as the Vertical Tab Strip expand/collapse, side
panel show/hide, etc.

It provides an interface for declaratively specifying animations, including
using (but not limited to) syntax similar to CSS.

## Motivation

Consider the toolbar-height side panel. When it shows, several things happen:
 - The panel itself expands
 - The area including the toolbar and the contents shrinks and moves over
 - A shadow appears around the toolbar and contents

These motions don't use the same animation curves, and they don't happen at
exactly the same time. This can be confusing to program and it often isn't
easy to translate the UX spec (which often uses CSS animation concepts) to a
Views animation.

**Browser Animation Controller** is designed to make it simple to define these
animations, and to allow views to subscribe for update events.

## Terminology

The primary entry point for this system is `BrowserAnimationController`. This is
[unowned user data](/ui/base/unowned_user_data/README.md) attached to a
`BrowserWindowInterface`, so it can be retrieved using:

```cpp
  auto* const controller = BrowserAnimationController::From(browser);
```

Animations are broken down into:
 1. **Group** - a set of related animations.
    For example:
    - Toolbar-height side panel
    - Contents-height side panel
    - Vertical tab strip
 2. **Motion** - a particular motion inside a group.
    For example, for the toolbar-height side panel, there are three motions:
    - Open
    - Open with content transition
    - Close
 3. **Sequence** - changes to a specific visual element that happen over the
    course of a motion. For example, when the toolbar-height side panel opens,
    the following may change:
    - Side panel width
    - Padding around the main area (toolbar, bookmarks, web contents)
    - Opacity of the shadow around the main area
 4. Individual parts of a sequence:
    - **Keyframe** - similar to CSS keyframes; these map a timestamp to an
      animation value
    - **Segment** - a subset of a motion during which an animation value changes
      to a new value
    - **Tween** - an interpolation curve used by either the entire motion, a
      single segment, or between two keyframes
    - **Snap** - the animation value jumps between specified values at a
      specific timestamp

**Groups**, **Motions**, and **Sequences** are named using
[unique identifiers](/ui/base/identifier/README.md): `BrowserAnimationGroup`,
`BrowserAnimationMotion`, and `BrowserAnimationSequence`, defined in
[browser_animation_types.h](./browser_animation_types.h). Specify these when
playing an animation or reading an animation value.

A `BrowserAnimationProvider` defines animations using a straightforward,
declarative syntax. For example, this (partial) example uses keyframes:

```cpp
  Group(kToolbarHeightSidePanelAnimationGroup,
        Motion(kSidePanelOpenMotion,
               // Animate the side panel for 350ms using an EASE_IN_OUT tween.
               Sequence(kSidePanelWidth,
                        Keyframe(AtMs(0), Value(0)),
                        Keyframe(AtMs(350), Value(1.0), gfx::Tween::EASE_IN_OUT)),
               // Animate the padding around the main contents area from 100 to
               // 450ms using an EASE_IN tween.
               Sequence(kMainAreaPaddingSize,
                        Keyframe(AtMs(100), Value(0)),
                        Keyframe(AtMs(450), Value(1.0), gfx::Tween::EASE_IN)),
               // Animate in the shadow opacity from 350ms to the end of the
               // animation using a LINEAR tween (i.e. no animation curve).
               Sequence(kMainAreaShadowOpacity,
                        Keyframe(AtMs(350), Value(0)),
                        Keyframe(AtMs(650), Value(1.0), gfx::Tween::LINEAR))),
        Motion(kSidePanelCloseMotion,
               // ...
```

All of the different ways to define an animation will be described later.

## Creating Animations

In order to create an animation, you will need to subclass either
`BrowserAnimationProvider` or `CachingBrowserAnimationProvider` and override a
single method.
  - `CachingBrowserAnimationProvider`: defines all of its animations once, in a
    single function.
  - `BrowserAnimationProvider`: computes animations on demand, whenever an
    animation is started.

The caching version is simpler, but some animations may need to change in
response to user or system preferences, or the state of the browser, so plain
`BrowserAnimationProvider` is provided for this case.

You register the animations using
`BrowserAnimationController::AddAnimationProvider()`. You should register the
provider once, before you need it:

```cpp
  BrowserAnimationController::From(browser)->AddAnimationProvider(
      std::make_unique<MyAnimations>());
```

Animation providers should be placed in
[chrome/browser/ui/views/animations](/chrome/browser/ui/views/animations).

Animations should be registered in
`BrowserWindowFeatures::InitPostBrowserViewConstruction()`.

You'll need to define your group(s), motions, and segments - either in your
provider class using `DECLARE/DEFINE_CLASS_BROWSER_ANIMATION_*()` macros, or in
a separate file using `DECLARE/DEFINE_BROWSER_ANIMATION_*()` macros. Putting
them in the class allows you to use much shorter identifier names and is
preferred.

### Defining Animations with `CachingBrowserAnimationProvider`

This is the easier of the two options.

Derive from `CachingBrowserAnimationProvider` and override
`GenerateAnimations()`:

```cpp
// In chrome/browser/ui/views/animations/my_animation_provider.h:

class MyAnimations : public CachingBrowserAnimationProvider {
 public:
  DECLARE_CLASS_BROWSER_ANIMATION_GROUP(kPanelGroup);
  DECLARE_CLASS_BROWSER_ANIMATION_GROUP(kFlyoverGroup);
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kExpandMotion);
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kCollapseMotion);
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kFadeInMotion);
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kFadeOutMotion);
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kPanelWidth);
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kElementHeight);
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kOpacity);

  GroupInfos GenerateAnimations() override;
};

// In chrome/browser/ui/views/animations/my_animation_provider.cc:

DEFINE_CLASS_BROWSER_ANIMATION_GROUP(MyAnimations, kPanelGroup);
DEFINE_CLASS_BROWSER_ANIMATION_GROUP(MyAnimations, kFlyoverGroup);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(MyAnimations, kExpandMotion);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(MyAnimations, kCollapseMotion);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(MyAnimations, kFadeInMotion);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(MyAnimations, kFadeOutMotion);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(MyAnimations, kPanelWidth);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(MyAnimations, kElementHeight);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(MyAnimations, kOpacity);

MyAnimations::GroupInfos
MyAnimations::GenerateAnimations() {
  return Groups(
      // Panel slides in and out while also growing vertically:
      Group(kPanelGroup,
            Motion(kExpandMotion, /* Insert sequences here. */),
            Motion(kCollapseMotion, /* Insert sequences here. */)),
      // Flyover expands/contracts and fades in/out:
      Group(kFlyoverGroup,
            Motion(kExpandMotion, /* Insert sequences here. */),
            Motion(kFadeOutMotion, /* Insert sequences here. */)));
}
```

See below for the syntax for sequences. Note that for
`CachingBrowserAnimationProvider`, `Motion()` requires specifying the
`BrowserAnimationMotion` as its first parameter.

### Defining Animations with `BrowserAnimationProvider`

This is the more complex of the two options, but provides the ability to
dynamically generate parameters as the state of the browser changes.

Derive from `BrowserAnimationProvider` and override `GetMotionSpecification()`.
You must manually determine which animation sequences to build based on the
`group` and `motion` parameters. Return `std::nullopt` if you do not provide
that motion in that group.

Example:

```cpp
// .h file:
class MyAnimations : public BrowserAnimationProvider {
 public:
  DECLARE_CLASS_BROWSER_ANIMATION_GROUP(kPanelGroup);
  DECLARE_CLASS_BROWSER_ANIMATION_GROUP(kFlyoverGroup);
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kExpandMotion);
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kCollapseMotion);
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kFadeInMotion);
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kFadeOutMotion);
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kPanelWidth);
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kElementHeight);
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kOpacity);

  std::optional<internal::MotionSpecification> GetMotionSpecification(
      BrowserAnimationGroup group,
      BrowserAnimationMotion motion) const override;
};

// .cc file:
DEFINE_CLASS_BROWSER_ANIMATION_GROUP(MyAnimations, kPanelGroup);
DEFINE_CLASS_BROWSER_ANIMATION_GROUP(MyAnimations, kFlyoverGroup);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(MyAnimations, kExpandMotion);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(MyAnimations, kCollapseMotion);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(MyAnimations, kFadeInMotion);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(MyAnimations, kFadeOutMotion);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(MyAnimations, kPanelWidth);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(MyAnimations, kElementHeight);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(MyAnimations, kOpacity);

std::optional<internal::MotionSpecification>
MyAnimations::GetMotionSpecification(
    BrowserAnimationGroup group,
    BrowserAnimationMotion motion) {
  // Have to explicitly check for groups and motions I support.
  if (group == kPanelGroup) {
    if (motion == kExpandMotion) {
      return Motion(/* Insert sequences here. */);
    } else if (motion == kCollapseMotion) {
      return Motion(/* Insert sequences here. */);
    }
  } else if (group == kFlyoverGroup) {
    if (motion == kFadeInMotion) {
      return Motion(/* Insert sequences here. */);
    } else if (motion == kFadeOutMotion) {
      return Motion(/* Insert sequences here. */);
    }
  }
  // I don't handle any other animations or motions, so return null.
  return std::nullopt;
}
```

See below for the syntax for sequences. Note that for
`BrowserAnimationProvider`, `Motion()` _does not require_ specifying the
`BrowserAnimationMotion` as its first parameter.

### What Happens When There Is More Than One Provider

Providers are checked from most-recently-added to least-recently-added. The
first provider that can provide an animation motion does so. This allows motions
to be overridden for e.g. testing by simply adding another provider after all
the others.

Normally, only one provider would provide a given motion.

## Defining Animation Sequences

There are four different types of ways to specify sequences:
 - **Snap:** animates from starting to ending value at a particular
   time/timestamp in the motion.
 - **Full-motion sequence:** animates from starting to ending value throughout
   the entire motion.
 - **Keyframe sequence:** value will hit each keyframe as the motion
   progresses
 - **Segment sequence:** starts at a specified value, then animate over one or
   more segments (subsets of the overall motion).

When specifying timestamps for snaps, keyframes, and segments, you can use two
different approaches:
 - Percentage of the overall animation.
 - Absolute time in milliseconds.

You cannot mix and match milliseconds and percentages within the same sequence
(exception: zero ms and zero percent are equivalent).

### Snap Sequences

Snap sequences provide a stepwise animation function.

Syntax for snap sequences is simple:

```cpp
  Snap(sequence_name,
       FromValue(starting_value), ToValue(ending_value),
       AtPercent(progress_percent));

  Snap(sequence_name,
       FromValue(starting_value), ToValue(ending_value),
       AtMs(absolute_time));
```

For example:

```cpp
  // Snap the top padding from 0% to 100% halfway through the motion.
  Snap(kTopPadding, FromValue(0.0), ToValue(1.0), AtPercent(0.5))
```

Snapping at the very start or end of an animation is supported, but often
unnecessary if the calling code is responsible for rendering when the animation
isn't playing.

### Full-Motion Sequences

A full-motion sequence uses the `Animate()` sequence declaration:

```cpp
  Animate(sequence_name,
          FromValue(starting_value), ToValue(ending_value)
          [, tween])
```

For example:

```cpp
  // Slide the panel out over the entire animation, using the global tween.
  Animate(kPanelWidth, FromValue(0.0), ToValue(1.0))

  // Slide the top of the panel down over the entire animation, applying an
  // ease in/out.
  Animate(kPanelTop, FromValue(0.0), ToValue(1.0), gfx::Tween::EASE_IN_OUT)
```

These will always play over the entire length of the animation. When using
`Animate()`, you must specify the motion's total duration (see below).

### Keyframe Sequences

A keyframe sequence is a series of `Keyframe()` declarations; similar to CSS:

```cpp
  Sequence(sequence_name,
           Keyframe(AtMs(first_keyframe_time),
                   Value(first_keyframe_value)),
           Keyframe(AtMs(second_keyframe_time),
                   Value(second_keyframe_value)
                   [, tween]),
           [, Keyframe(...)]);

  Sequence(sequence_name,
           Keyframe(AtPercent(first_keyframe_percent),
                    Value(first_keyframe_value)),
           Keyframe(AtPercent(second_keyframe_percent),
                   Value(second_keyframe_value)
                   [, tween]),
           [, Keyframe(...)]);
```

The animation value will start at the value of the first keyframe, and end at
the value of the last keyframe. You may specify keyframes at the start (time
zero) and end of the animation.

Keyframes must be specified in order of time.

If two subsequent keyframes have the same value, no animation takes place
between them.

If keyframes are specified in terms of percentage, you must specify the motion's
total duration (see below). Keyframes in the same sequence cannot mix and match
percentage and absolute time.

#### Keyframe Sequence Example

```cpp
  Sequence(kMyAnimationElement,
           Keyframe(AtMs(100), Value(0)),
           Keyframe(AtMs(350), Value(0.7), gfx::Tween::EASE_IN_OUT),
           Keyframe(AtMs(500), Value(0.7)),
           Keyframe(AtMs(750), Value(1.0), gfx::Tween::LINEAR))
```

In this example:
 - The animation starts at 0.
 - From 100ms to 350ms it animates to 0.7 using ease-in-out.
 - From 350ms to 500ms it remains at 0.7.
 - From 500ms to 750ms it animates linearly to 1.0.
 - At 750ms this element is finished animating.

### Segment Sequences

A segment sequence has a defined starting value, then only animates over
specific segments, which are subsets of the motion.

Syntax for segment sequences is:

```cpp
  Sequence(sequence_name,
           StartingAt(initial_value),
           Segment(StartPercent(first_segment_start),
                   EndPercent(first_segment_end),
                   AnimateTo(first_segment_end_value)
                   [, tween]),
           Segment(StartPercent(second_segment_start),
                   EndPercent(second_segment_end),
                   AnimateTo(second_segment_end_value)
                   [, tween])
           [, Segment(...)])
```

Sequences must be in order and cannot overlap, though the end of one segment can
be the beginning of the next.

If segments are specified using percentages, you must specify the motion's total
duration (see below). Segments in the same sequence cannot mix and match percent
and absolute time.

#### Segment Sequence Example:

The exact same animation as in the keyframe example above can be expressed using
segments:

```cpp
  Element(kMyAnimationElement,
          StartingValue(0.0),
          Segment(StartMs(100), EndMs(350),
                  AnimateTo(0.7), gfx::Tween::EASE_IN_OUT),
          Segment(StartMs(500), LengthMs(250),
                  AnimateTo(1.0), gfx::Tween::LINEAR))
```

Keyframes and segments are functionally equivalent so use whichever one makes
more sense for your situation.

### Motion Length and Global Tween

You may optionally specify two additional values when defining a motion:
a motion length, using `TotalDurationMs(motion_length)` and a global tween.
These go immediately before the sequences.

If you do not specify a motion length and tween, then:
 - All segments must be in absolute time.
 - The length of the motion is defined by the largest timestamp in a sequence.

If you do specify a motion length and tween, then:
 - All percentages specified in segments are relative to that length and tween.
 - Tweens in percentage segments are applied _on top of_ the global tween.
   - Make the global tween `LINEAR` to avoid layering tweens in this way.
 - Absolute time segments ignore the global tween.
 - Absolute time segments cannot be longer than the motion's total duration.

Percent and absolute time cannot be mixed in the same keyframe or segment
sequence (with the caveat that zero ms and zero percent are equivalent), but you
can mix and match sequences that use percentage and absolute time.

#### Motion Length and Global Tween Example

```cpp
  Motion(// Note: motion id is only required for caching provider; omit this if
         // extending `BrowserAnimationProvider`:
         motion_id,
         // The motion lasts for exactly 1s.
         TotalDurationMs(1000),
         // All segments with percentages will follow an ease tween.
         gfx::Tween::EASE_IN_OUT,
         // This sequence is in percent and will use the global tween, animating
         // between 0.0 and 1.0 when the output of the global tween is between
         // 0.1 and 0.9.
         Sequence(kPanelWidth,
                  StartingAt(0.0),
                  Segment(StartPercent(0.1), EndPercent(0.9), AnimateTo(1.0))),
         // This sequence is in absolute time and will ignore the global tween.
         Sequence(kFloatingElement,
                  Keyframe(AtMs(0), Value(0.0)),
                  Keyframe(AtMs(250), Value(1.0), gfx::Tween::EASE_OUT)),
         // This sequence will play over the entire 1000 ms and will use the
         // global tween.
         Animate(kSomeOtherThing, FromValue(1.0), ToValue(0.0)))
```

## Using `BrowserAnimationController`

Once the animation provider is registered, you can start an animation motion via
`Start()`. Example:

```cpp
  BrowserAnimationController::From(browser)->Start(
      MyAnimations::kPanelGroup, MyAnimations::kExpandMotion);
```

You can then get the value of an animation using `GetCurrentValue()`. Sometimes,
it's sufficient to just call this during `Layout()` or `OnPaint()`.

```cpp
  const auto value = BrowserAnimationController::From(browser)->GetCurrentValue(
      MyAnimations::kPanelGroup, MyAnimations::kPanelWidth);
  if (auto.has_value()) {
    // The side panel is animating; the value is valid.
  } else {
    // The side panel isn't animating; use the default width for the panel's
    // current state.
  }
```

Other times, you need to know when the animation has changed so you can e.g.
invalidate a layout, or update a size or opacity. In this case, use
`Subscribe()`:

```cpp
  MyClass() {
    // This will only trigger the callback for cases where the "panel group" is
    // animating; other animations won't trigger callbacks.
    animation_subscription_ =
        BrowserAnimationController::From(browser)->Subscribe(
            MyAnimations::kPanelGroup,
            base::BindRepeating(&MyClass::OnAnimationUpdated,
                                base::Unretained(this)));
  }

  void OnAnimationUpdated(const BrowserAnimationController* controller,
                          BrowserAnimationUpdate status) {
    // View grows or shrinks based on the current animation value.
    PreferredSizeChanged();

    // If I need to directly update a view property in this method, I can call
    // `controller->GetCurrentValue()` to see where the animation is.
  }
```

The `status` will either be "progressed", "ended", or "canceled". You can use
this if you need it; note that if the animation is canceled, calling
`GetCurrentValue()` will return null. Calling `GetCurrentValue()` when the
animation ends will always give the final value for the sequence.
