# Browser Animation Controller

This is a browser-window-level service which coordinates multi-View and
multi-part animations, such as the Vertical Tab Strip expand/collapse, side
panel show/hide, etc.

It provides an interface for declaratively specifying animations, including
using (but not limited to) syntax similar to CSS.

It allows for smooth redirection of animations, even mid-animation.

Finally, it allows for automatic logging of performance data to histograms.

## Motivation

Consider the toolbar-height side panel. When it shows, several things happen:
 - The panel itself expands
 - The area including the toolbar and the contents shrinks and moves over
 - A shadow appears around the toolbar and contents

These motions don't use the same animation curves, and they don't happen at
exactly the same time. This can be confusing to program and it often isn't
easy to translate the UX spec (which often uses CSS animation concepts) to a
Views animation.

Furthermore, if the panel is in the middle of closing and then decides to open
again, you have to write special-case code to blend the two animations together.

**Browser Animation Controller** is designed to make it simple to define these
animations, have them behave correctly, and to allow views to subscribe for
update events, without having to write any special-case code.

## Terminology

The primary entry point for this system is `BrowserAnimationController`. This is
[unowned user data](/ui/base/unowned_user_data/README.md) attached to a
`BrowserWindowInterface`, so it can be retrieved using:

```cpp
  auto* const controller =
      BrowserAnimationController::From(browser_window_interface);
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
    course of a motion.
    - For example, when the toolbar-height side panel opens, the following may
      change:
      - Side panel width
      - Padding around the main area (toolbar, bookmarks, web contents)
      - Opacity of the shadow around the main area
    - You can also specify if a sequence value persists after a motion
      completes, what the default value is, and if it returns automatically to
      that default value, etc.
 4. Individual parts of a sequence:
    - **Keyframe** - similar to CSS keyframes; these map a timestamp to an
      animation value
    - **Segment** - a subset of a motion during which an animation value changes
      to a new value (time between two keyframes)
    - **Tween** - an interpolation curve used by either the entire motion, a
      single segment, or between two keyframes
    - **Snap** - the animation value jumps between specified values at a
      specific timestamp
 5. **Transition** - how sequence values modified by one motion and persisting
    afterwards participate in the next motion, allowing animations to flow into
    each other.

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
  - `CachingBrowserAnimationProvider` **(recommended)**: defines all of its
    animations once, in a single function.
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
[chrome/browser/ui/views/animations](/chrome/browser/ui/views/animations). There
are several examples already there you can use as guides.

Animations should be registered in
`BrowserWindowFeatures::InitPostBrowserViewConstruction()`.

You'll need to define your group(s), motions, and segments - either in your
provider class using `DECLARE/DEFINE_CLASS_BROWSER_ANIMATION_*()` macros, or in
a separate file using `DECLARE/DEFINE_BROWSER_ANIMATION_*()` macros. Putting
them in the class allows you to use much shorter identifier names and is
preferred.

### Defining Animations with `CachingBrowserAnimationProvider`

This is the easier (and recommended) of the two options.

Derive from `CachingBrowserAnimationProvider` and override
`GenerateAnimations()`:

```cpp
// In chrome/browser/ui/views/animations/my_animation_provider.h:

class MyAnimations : public CachingBrowserAnimationProvider {
 public:
  // Required to support retrieval.
  DECLARE_FRAMEWORK_SPECIFIC_IMPLEMENTATION()

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

DEFINE_FRAMEWORK_SPECIFIC_IMPLEMENTATION(MyAnimations)

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

Derive from `BrowserAnimationProvider` and override
`GetMotionSpecificationImpl()`. You must manually determine which animation
sequences to build based on the `group` and `motion` parameters. Return
`std::nullopt` if you do not provide that motion in that group.

Example:

```cpp
// .h file:
class MyAnimations : public BrowserAnimationProvider {
 public:
  // Required to support retrieval.
  DECLARE_FRAMEWORK_SPECIFIC_IMPLEMENTATION()

  DECLARE_CLASS_BROWSER_ANIMATION_GROUP(kPanelGroup);
  DECLARE_CLASS_BROWSER_ANIMATION_GROUP(kFlyoverGroup);
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kExpandMotion);
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kCollapseMotion);
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kFadeInMotion);
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kFadeOutMotion);
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kPanelWidth);
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kElementHeight);
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kOpacity);

  std::optional<internal::MotionSpecification> GetMotionSpecificationImpl(
      BrowserAnimationGroup group,
      BrowserAnimationMotion motion) const override;
};

// .cc file:
DEFINE_FRAMEWORK_SPECIFIC_IMPLEMENTATION(MyAnimations)

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

You may snap at the very start or end of a motion.

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

Keyframes must be specified in order of time. If two subsequent keyframes are at
the exact same time, the value of the sequence will jump from the first value to
the second.

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

Segments must be in order and cannot overlap, though the end of one segment can
be the beginning of the next.

You may specify a length instead of an end.

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

## Sequence Properties (Defaults, Persistence, and Transitions)

By default, once a motion has completed, `GetCurrentValue()` will return null.
This means you will need to compute all the visual properties of your UI
elements whenever an animation isn't playing.

If you want sequence values to persist between motions and even be factored into
future motions, you can specify sequence properties. You do not need to specify
properties for all sequences in your `BrowserAnimationProvider`; merely those
that need to not have the default behavior described above.

For example, you might want to have the system remember your side panel width
between animations, so that (a) you don't have to query the panel state when
it's not animating and (b) if you start a close in the middle of an open or
vice-versa, the animation starts from its current position (rather than fully
open or closed).

### Configuring Sequences

You'll want to create a `SequenceParams` in your animation provider for each
such sequence. These have three properties:
 - `persist_between_animations` - if set to `true`, the value is not discarded
   at the end of a motion. Default is false.
 - `default_value` - specifies a value that will be returned if there is no
   known value, or the current value is explicitly default (see below). Default
   is none.
 - `auto_return_to_default` - whether a motion which does not specify an
   animation for this sequence should automatically return it to its default
   value. Default is false.

For `BrowserAnimationProvider`, override `GetAllSequenceParams()` and optionally
`GetSequenceParams()`. These require directly using `SequenceParams`.

For `CachingBrowserAnimationProvider` (recommended), you should instead use
`SetSequenceParams()`, typically in the constructor. This lets you use the much
simpler `Persist()` and `Default()` to define params.

```cpp

MyAnimations::MyAnimations() {
  SetSequenceParams(
    // Parameters for the panel group.
    kPanelGroup,
    // Panel width value persists between motions.
    Persist(kPanelWidth),
    // Element height persists and returns to a default value of 1 if not
    // specified in a motion.
    Default(kElementHeight, 1.0, /*auto_return=*/true));
}

```

### Using the Default Value

In addition to a literal value (e.g. "1.0", "0.0", "0.5") you can specify that
the default value should be used for a keyframe or animation endpoint:

```cpp
    Motion(kExpandMotion,
           Animate(kOpacity, FromValue(0.0), ToValue(DefaultValue())))
```

This will animate to whatever the default opacity is (you must have already
defined it). If you want to create a more complex animation curve but don't want
to exceed the default value, you can do something like:

```cpp
    Motion(kExpandMotion,
           Sequence(
              kOpacity,
              // Start at zero.
              StartingValue(0.0),
              // Pause at 0.7 or default value, whichever is less.
              Segment(StartMs(0), EndMs(200), ToValue(MinOfDefaultAnd(0.7))),
              // Animate to the default value.
              Segment(StartMs(800), EndMs(1000), ToValue(DefaultValue()))))
```

### Updating the Default Value

Sequence properties including the default value may change, e.g. via
`CachingBrowserAnimationProvider::UpdateDefaultValue()`. You might use this if,
for example, the user changes a setting that affects the UI. Note that you can
retrieve an animation provider from the animation controller by type:

```cpp
void OnUsePanelTransparencyChanged(bool use_transparency) {
  auto* const controller = BrowserAnimationController::From(browser);
  MyAnimations* const animations = controller->GetAnimationProvider<MyAnimations>();
  animations->UpdateDefaultValue(
      MyAnimations::kPanelGroup,
      MyAnimations::kOpacity,
      use_transparency ? 0.8 : 1.0);
}
```

This won't only affect the target opacity for future animations - if the current
value of `kOpacity` is the default, any calls to
`BrowserAnimationController::GetCurrentValue(kPanelGroup, kOpacity)` will return
the updated value.

### Return and Auto-Return

You can specify that a sequence should explicitly return from the current value
to some specific value using the `Return()` sequence specification. The value
must use `Persist()` or `Default()` since otherwise there's no value to return
from:

```cpp
    Motion(kCollapseMotion,
           Return(kPanelWidth, ToValue(0.0)),
           ...)
```

Basically, if you don't care where the animation starts from, use `Return()`.

**Auto-return** can happen when you specify something like
`Default(kElementHeight, 1.0, /*auto_return=*/true)` and you don't specify
what to do with `kElementHeight` in a particular motion. When that motion is
played, the following specification will be implicitly added:

```cpp
  Return(kElementHeight, ToValue(DefaultValue()))
```

### Transition Types

When there is an existing value that has persisted, a **transition** plays.
The default transition is "start at", but there are three options, which can
be specified in most sequences:
 - **Start at** - scales the animation so that it starts at the previous value.
 - **Cap at** - plays the animation as normal, but bounds the value between
   the previous value and the final (target) value.
 - **Ignore** - just plays the motion, ignoring the current value. Current
   values still persist between motions.

Consider a segment which tweens linearly from 0 to 1, but the current value is
0.5. Here's the result with each transition type:

| Transition | before | 0%   | 25%  | 50%  | 75%  | 100% |
| ---------- | ------ | ---- | ---- | ---- | ---- | ---- |
| start at   | 0.5    | 0.5  | 0.68 | 0.75 | 0.89 | 1    |
| cap at     | 0.5    | 0.5  | 0.5  | 0.5  | 0.75 | 1    |
| ignore     | 0.5    | 0    | 0.25 | 0.5  | 0.75 | 1    |

If _start at_ or _cap at_ is specified, but the starting value is outside the
range of the animation, a crossfade is played between the current value and the
animation over the course of the motion:

| Transition | before | 0%   | 25%  | 50%  | 75%  | 100% |
| ---------- | ------ | ---- | ---- | ---- | ---- | ---- |
| start at   | -1     | -1   | -0.5 | 0    | 0.5  | 1    |
| cap at     | -1     | -1   | -0.5 | 0    | 0.5  | 1    |
| ignore     | -1     | 0    | 0.25 | 0.5  | 0.75 | 1    |

You may explicitly specify a transition as the second parameter of a `Segment()`
declaration. You never have to specify `kStartAtOldValue` as it is the default.

```cpp
  Segment(kOpacity, Transition::kCapAtOldValue, Keyframe(...), ...)
  Segment(kOpacity, Transition::kIgnoreOldValue,
          StartingValue(0.0), Segment(...), ...)
```

## Using `BrowserAnimationController`

Once your animation provider is registered, you can start an animation motion
via `Start()`. Example:

```cpp
  BrowserAnimationController::From(browser)->Start(
      MyAnimations::kPanelGroup, MyAnimations::kExpandMotion);
```

You can then get the value of an animation using `GetCurrentValue()`. Sometimes,
it's sufficient to just call this during `Layout()` or `OnPaint()`.

```cpp
  const auto anim_value =
      BrowserAnimationController::From(browser)->GetCurrentValue(
          MyAnimations::kPanelGroup, MyAnimations::kPanelWidth);

  // Note: if we always call `Reset()` during system initialization, then
  // `kPanelWidth` - which is persisted - would always have a valid value, and
  // we wouldn't need `value_or()` here.
  const int side_panel_width =
      base::ClampRound(preferred_side_panel_width * anim_value.value_or(1.0));

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

The `status` will be "started", "progressed", "ended", or "canceled". Calling
`GetCurrentValue()` when the animation ends will always yield the final value
for the sequence.

### Resetting and Clearing Animations

The `Clear()` method clears all values and stops all motions, subsequently,
`GetCurrentValue()` will return null (at least until the next call to
`Start()`), or a default value if one has been set.

The `Reset()` method immediately stops all motions and snaps the state of all
sequences to the end of a specified motion. Only sequences marked as persist
will be saved; all others will be null/default as with `Clear()`.

`Reset()` is useful when you want to snap directly to a state. If you do not
specify a motion, then the values will snap to the end of the current motion
(if there is no current motion this does nothing).

```cpp
  // Immediately collapse the panel.
  auto* const controller = BrowserAnimationController::From(bwi);
  controller->Reset(kPanelGroup, kCollapseMotion);
```

`Clear()` generates a _cancel_ event if a motion is playing. `Reset()` may
generate a _cancel_ event if a different motion is playing, and will generate
an _ended_ event if the target motion is valid. During the _ended_ event, all
sequence values will be the final values of the motion.

## Automatic Logging to Histograms

There are two automatic histograms that can be emitted for each motion:
 - Frames per second
 - Longest frame time in milliseconds

In order to have these histograms output you need to do the following:
 1. Generate the names for the relevant group and motion in your
    `BrowserAnimationProvider`.
 2. Add the histograms to a `histograms.xml` file. You can use variants for your
    group and/or motion names to simplify the declarations.

### Generating Histogram Names

As always, we recommend using `CachingBrowserAnimationProvider`. In your
constructor, call `SetHistogramName()` for your groups and motions:

```cpp

MyAnimations::MyAnimations() {
  // SetSequenceParams(...);

  // LINT.IfChange(MyFeatureHistogramsNames)

  // Name group histogram prefixes:
  SetHistogramName(kPanelGroup, "MyFeature.PanelAnimations");
  SetHistogramName(kFlyoverGroup, "MyFeature.FlyoverAnimations");

  // Name motion histogram infixes:
  SetHistogramName(kExpandMotion, "Expand");
  SetHistogramName(kCollapseMotion, "Collapse");
  SetHistogramName(kExpandMotion, "FadeIn");
  SetHistogramName(kCollapseMotion, "FadeOut");

  // LINT.ThenChange(MyFeatureHistogramsDeclarations)
}

```

Some notes:
 - The name of a group or motion may contain a dot. In this case, the
   histogram names for a panel expand motion would be:
   - `MyFeature.PanelAnimations.Expand.AnimationFPS`
   - `MyFeature.PanelAnimations.Expand.TimeOfLongestAnimationStep`
- If no name has been specified for either the group, motion, or both, no
  histogram will be emitted.
- If a name is specified but empty, a dot will not be inserted:
  - "", "MyMotion" -> "MyMotion.AnimationFPS"
  - "MyGroup", "" -> "MyGroup.AnimationFPS"
  - "", "" -> _illegal; will generate an error_
- Always use linter directives to avoid getting out of sync with histogram
  declarations.

#### Specifying/Overriding Histogram Names

You can specify or override the histogram name of either the group or motion
(or both) when starting an animation. This may cause a histogram to be emitted
where it might not otherwise be. Example:

```cpp
  // This will cause the following performance histograms to be emitted:
  //  - AlternativeGroupPrefix.AlternativeMotionInfix.AnimationFPS
  //  - AlternativeGroupPrefix.AlternativeMotionInfix.TimeOfLongestAnimationStep
  //
  // It does not matter if the provider registered histograms for this group or
  // motion previously.
  controller->Start(
      kPanelGroup, kExpandMotion,
      "AlternativeGroupPrefix",
      "AlternativeMotionInfix");
```

### Declaring Histograms

It is currently best practice to declare your histograms so that the
prefix/first term is the histogram prefix for your feature. This prevents
multiplying prefixes, which is against best practices.

```xml

<!-- LINT.IfChange(MyFeatureHistogramsDeclarations) -->

<variants name="MyFeatureAnimationGroups">
  <variant name="PanelAnimations" summary="..."/>
  <variant name="FlyoverAnimations" summary="..."/>
</variants>

<variants name="MyFeatureAnimationMotions">
  <variant name="Expand" summary="..."/>
  <variant name="Collapse" summary="..."/>
  <variant name="FadeIn" summary="..."/>
  <variant name="FadeOut" summary="..."/>
</variants>

<!-- LINT.ThenChange(MyFeatureHistogramNames) -->

<histogram
    name="MyFeature.{MyFeatureAnimationGroups}.{MyFeatureAnimationMotions}.AnimationFPS"
    units="fps"
    expires_after="2099-01-01">
  <owner>email@chromium.org</owner>
  <summary>
    Records the frames per second for MyFeature animations.
    Recorded when an animation completes.
  </summary>
  <token key="MyFeatureAnimationGroups" variants="MyFeatureAnimationGroups"/>
  <token key="MyFeatureAnimationMotions" variants="MyFeatureAnimationMotions"/>
</histogram>

<histogram
    name="MyFeature.{MyFeatureAnimationGroups}.{MyFeatureAnimationMotions}.TimeOfLongestAnimationStep"
    units="ms"
    expires_after="2099-01-01">
  <owner>email@chromium.org</owner>
  <summary>
    Records the longest frame in an animation in ms.
    Recorded when an animation completes.
  </summary>
  <token key="MyFeatureAnimationGroups" variants="MyFeatureAnimationGroups"/>
  <token key="MyFeatureAnimationMotions" variants="MyFeatureAnimationMotions"/>
</histogram>

```

## Best Practices

Persist all values that make sense to. This will make animations smoother.
All things equal, also prefer to enable auto-return to make your motions
simpler.

If your UI can be configured to start in one of several initial states (such as
vertical tab strip expanded or collapsed mode), call `Reset()` to set up the
current state on UI initialization. That way, each sequence will always have a
valid value that reflects the current state.

When choosing which state to pick "default" values from, think of the state that
the most things can transition to/from. For example, all animations in vertical
tab strip start or end at the collapsed state.

## Possible Future Work

- Instead of one default state, have multiple states, with each animation ending
  in a state.
  - Keyframe values can reference one or more states rather than just the
    default.
  - Each state can be modified in response to e.g. user preference or window
    geometry changes.
- Allow more complicated math when computing keyframe values.
- Create a common, default histogram prefix for animation performance
  histograms.