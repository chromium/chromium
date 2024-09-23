This directory defines the ChromeOS API (crosapi). This is the
communication protocol between lacros (web-browser on ChromeOS) and ash
(user-space system executable on ChromeOS) for all new IPCs. Some existing IPCs
might use Wayland or D-Bus to avoid unnecessary rewrites.

The ChromeOS API is eventually going to be stabilized and will need to tolerate
several milestones worth of version skew between lacros and ash. In the long
term, these interfaces will potentially need to support years of version skew.
This means that any mojom files and their transitive dependencies must be
relatively stable and backwards compatible. By default, mojom files in this
directory should not include mojoms from any other directories unless they are
marked \[Stable\].

Note: The mojom subdirectory contains the stable API. The cpp subdirectory holds
helper c++ code, but is not part of the API surface itself.

# Guidelines

## When to use Crosapi vs Wayland?
Wayland should be used when implementing features that primarily deal with
display and input. Wayland should be engaged when dealing with

* Window management
* Surface position, window state (fullscreen, minimized, etc)
* Display management (scaling, display layout, etc)
* Input Handling (keyboard, mouse, IME, etc)
* Compositing
* Handling of graphics primitives

Concerns that do not fall into these buckets are candidates for the mojo-based
crosapi.

Clients should avoid mixing use of the two APIs for a given feature (and this
should not be necessary in most circumstances). Ordering of messages is
guaranteed within each crosapi interface and globally for wayland, however there
is no such sequential guarantee of ordering between the two protocols.

Consider the example of a feature wanting to keep updated captures of graphical
browser contents to improve user restore experience on the OS level.

Depending on requirements Wayland may be a good fit. Changes to browser contents
can be detected by Ash via tracking damage incurred on Wayland surfaces. Frames
produced by these surfaces could then be persisted for product use-cases by the
OS.

However Wayland does not expose higher level browser logic or state. To avoid
frequently updating stored captures for inconsequential updates it may instead
make sense to notify the OS of the specific browser-side triggers (navigation,
load completion for e.g.) and persist frames when these are received by the OS.
Such an API must be added to the mojom crosapi.

## Create a Crosapi subservice for your feature
Create a Crosapi subservice for your feature. Some examples of features include:

* A service that restores the browser state.
* Theme synchronization between ChromeOS and the browser.
* Background job that collects browser metrics and reports it under a ChromeOS
identifier.

Organize feature code into their own module/directory. For example, if there is
a feature called Foo, create a new module called crosapi.foo.mojom. Create a new
directory under [chromeos/crosapi/mojom/](/chromeos/crosapi/mojom/) to contain
the code for this module. It might make sense to create additional submodules to
further organize the feature service.

## Adding new fields to structs and new parameters to methods
Refer to Mojo’s
[versioning instructions](/mojo/public/tools/bindings/README.md#Versioning).
All new fields are required to be optionals with the exception of primitive
numeric type. Primitive numeric types can be nullable or non-nullable, but
special considerations need to be made about the semantics of non-nullable
default values.

Refer to [mojo’s backwards compatibility documentation](/mojo/public/tools/bindings/README.md#ensuring-backward-compatible-behavior)
when choosing between nullable and non-nullable primitive types. Prefer nullable
primitives if you are uncertain about the consequences of using a non-nullable
primitive.

## Defining mojo enums
Always define enums as [Extensible] and include a kUnknown. This is important because the enum list might grow in the future which can cause issues when the versions mismatch. Example:

```
[Extensible]
enum Status {
    [Default] kUnknown = 0,
    kSuccess = 1,
    kFailed = 2,
}
```

This is important for the caller or receiver to understand if there is a possible unanticipated request or response. When a service receives an enum values that it does not recognize, it will automatically set the enum value to kUnknown. Some patterns to avoid.

```
// This pattern will turn a failure into success if there is a version mismatch
// and could lead to unexpected behaviours.
[Extensible]
enum Status {
    [Default] kSuccess = 0,
    kFailureType1 = 1,
    kFailureType2 = 2,
    kFailureType3 = 3,
}

// kNone is ambiguous here. Is kNone an okay value to set when a new type of hardware is
// introduced? Does kNone have a behaviour associated with it that is different from an
// unknown device type?
[Extensible]
enum DeviceType {
    [Default] kNone = 0,
    kMouse = 1,
    kKeyboard = 2,
    kOther = 3,
}
```

Instead, do:

```
// Unknown can be specially handled. Maybe the feature needs to be disabled in this
// case.
[Extensible]
enum Status {
    [Default] kUnknown = 0,
    kSuccess = 1,
    kFailureType1 = 2,
    kFailureType2 = 3,
    kFailureType3 = 4,
}

// If we receive an unknown device type, we might want to inform the user to update or that
// it is an unsupported device type.
[Extensible]
enum DeviceType {
    [Default] kUnknown = 0,
    kNone = 1,
    kMouse = 2,
    kKeyboard = 3,
    kOther = 4,
}
```
