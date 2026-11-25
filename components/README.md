# About //components

This directory is meant to house features or subsystems that are used in more
than one part of the Chromium codebase (as described below). It is acceptable
to bring up a feature in //components that has a single initial use case but
for which a second use case is targeted (e.g., a feature that is initially
used on Chrome Desktop but for which integration on Chrome for iOS is
planned in the future). In that case, if the planned second use case ends up not
occurring when the dust settles, the feature team and/or eng from the relevant
platform team should loop back to do the relevant architectural
simplifications (e.g., eliminating the component being layered, moving the
code to //chrome if it becomes clear that it's actually a desktop Chrome-only
feature).

## Use cases:

  * Features that are shared by Chrome on iOS (`//ios/chrome`) and Chrome on
    other platforms (`//chrome`).
      * Note: `//ios` doesn't depend on `//chrome`.
  * Features that are shared between multiple embedders of content. For example,
    `//chrome` and `//android_webview`.
  * Features that are shared between Blink and the browser process.
      * Note: It is also possible to place code shared between Blink and the
        browser process into `//third_party/blink/common`. The distinction comes
        down to (a) whether Blink is the owner of the code in question or a
        consumer of it and (b) whether the code in question is shared by Chrome
        on iOS as well. If the code is conceptually its own cross-process
        feature with Blink as a consumer, then `//components` can make sense. If
        it's conceptually Blink code, then `//third_party/blink/common` likely
        makes more sense. (In the so-far hypothetical case where it's
        conceptually Blink code that is shared by iOS, raise the question on
        chromium-dev@, where the right folks will see it).

Note that the above list is meant to be exhaustive. A component should not be
added just to separate it from other code in the same layer that is the only
consumer; that can be done with strict `DEPS` and GN `visibility` rules. In
particular, features that are added to //chrome can (and should) have
proper modularity both between each other and between //chrome glue code such
as Profile.

## Before adding a new component

  * Is there an existing component that you can leverage instead of introducing
    a new component?
      * Can you restructure an existing component to logically encompass the
        proposed new code?
      * As a general rule, we prefer fewer top level components. So, consider
        whether adding sub-features within an existing component is more
        appropriate for your use case.
      * Historically, dependency issues were simply addressed by adding new
        components. But, you can (and it is preferred to) solve that by
        restructing an existing component and its dependencies where possible.

## Guidelines for adding a new component

  * You will be added to an `OWNERS` file under `//components/{your component}`
    and be responsible for maintaining your addition.
      * You must specify at least two OWNERS for any new component.
  * A `//components/OWNER` must approve of the location of your code.
  * The CL (either commit message or comment) must explicitly specify what [use
    case(s)](#use-cases) justify the new component.
  * Code must be needed in at least 2 places in Chrome that don't have a "higher
    layered" directory that could facilitate sharing (e.g. `//content/common`,
    `//chrome/utility`, etc.).
  * The CL adding a new component should be substantial enough so that
    //components/OWNERS can see its basic intended structure and usage before
    approving the addition (e.g., it should not just be an empty shell).
  * You must add a [`DIR_METADATA`](https://source.chromium.org/chromium/infra/infra/+/main:go/src/infra/tools/dirmd/README.md)
    file under `//components/{your component}` with an appropriately specified
    bug-component.

## Dependencies of a component

Components **cannot** depend on the higher layers of the Chromium codebase:

  * `//android_webview`
  * `//chrome`
  * `//chromecast`
  * `//headless`
  * `//ios/chrome`
  * `//ios/web_view`
  * `//content/shell`

Components **can** depend on the lower layers of the Chromium codebase:

  * `//base`
  * `//gpu`
  * `//mojo`
  * `//net`
  * `//printing`
  * `//ui`

Components **can** depend on each other. This must be made explicit in the
`DEPS` file of the component.

Components **can** depend on `//content/public`, `//ipc`, and
`//third_party/blink/public` (or `//ios/web/public` on iOS). This must be made
explicit in the `DEPS` file of the component.

However, code that is shared with iOS (which does not use `//content`, `//ipc`,
or Blink) *cannot* depend on `//content/public`, `//ipc`, or
`//third_party/blink/public` (nor can shared cross-platform code depend on
`//ios/web/public`). It is not an acceptable solution to conditionally depend on
these modules in code shared with iOS; any platform-specific dependencies must
instead be injected into the shared code.

The most straightforward way to achieve this is to keep the component free of
`//content`, `//ipc`, `//third_party/blink`, and `//ios/web` dependencies and
inject platform-specific dependencies directly from the embedders (via
constructor parameters, callbacks, or narrow delegate/client interfaces).
Embedder-specific wiring and leaves (e.g., `KeyedService` factories, tab-scoped
feature/helper registration, Android JNI bridges, and platform-specific UI or
web-layer interactions) live in the embedder (`//chrome`, `//ios/chrome`, etc.).

The above is recommended as the default approach (for example, for components
shared only between `//chrome` and `//ios/chrome`). However, there are cases
where that approach results in an undesirable amount of code duplication in
embedders (e.g., when multiple `//content` embedders or multiple `//ios/web`
embedders embed the component). In those cases, a [layered
component](https://www.chromium.org/developers/design-documents/layered-components-design)
is an alternative option. Because layered components introduce significant
complexity, they should be used only when sharing platform-layer glue is a
clear net win:

  * A `content/` layer (separating `//content`, `//ipc`, or Blink dependencies
    from platform-agnostic `core/` code) should be used **only** if the
    component is shared (either directly or transitively) **both** with iOS
    **and** with at least one `//content` embedder beyond `//chrome` (e.g.,
    `//android_webview`), **and** there are sufficient interactions with
    `//content` (or `//ipc`/`//third_party/blink`) to justify sharing that glue
    rather than duplicating it across those embedders.
  * Symmetrically, an `ios/` layer for the purpose of depending on `//ios/web`
    should be used **only** if the component is shared (either directly or
    transitively) across platforms **and** with multiple `//ios/web` embedders
    (e.g., `//ios/chrome` and `//ios/web_view`), **and** there are sufficient
    interactions with `//ios/web` to justify sharing that glue rather than
    duplicating it across those embedders. (Code shared only among `//ios/web`
    embedders belongs in `//ios/components/` instead.)
  * Having one of these layers does not require having the other: for example, a
    component with an `ios/` layer shared by `//ios/chrome` and `//ios/web_view`
    still does not need a `content/` layer if `//chrome` is its only `//content`
    embedder (and vice versa).

`//chrome`, `//ios/chrome`, `//content` and `//ios/web` **can** depend on
individual components. The dependency might have to be made explicit in the
`DEPS` file of the higher layer (e.g. in `//content/browser/DEPS`). Circular
dependencies are not allowed: if `//content` depends on a component, then that
component cannot depend on `//content/public`, directly or indirectly (and
similarly for `//ios/web` and `//ios/web/public`).

## Structure of a component

Only [layered
components](https://www.chromium.org/developers/design-documents/layered-components-design)
(when justified by the criteria above) need to separate platform-agnostic code
into a `core/` subdirectory alongside a `content/` and/or `ios/` subdirectory.
A component that is not layered (including the common case of a component shared
only between `//chrome` and `//ios/chrome`) does **not** need a `core/`
subdirectory; its code can live directly under `components/foo/` (with process
subdirectories underneath if needed).

Components that have bits of code that need to live in different processes (e.g.
some code in the browser process, some in the renderer process, etc.) should
separate the code into different subdirectories. Hence for a non-layered
component named 'foo' you might end up with a structure like the following:

  * `components/foo`          - `BUILD.gn`, `DEPS`, `DIR_METADATA`, `OWNERS`, `README.md`
  * `components/foo/browser`  - code that needs the browser process
  * `components/foo/common`   - for e.g. Mojo interfaces and such
  * `components/foo/renderer` - code that needs renderer process

These subdirectories should have `DEPS` files with the relevant restrictions in
place; e.g., assuming that `foo` is not shared with iOS, only
`components/foo/browser` should be allowed to `#include` from
`content/public/browser` (if such a component **is** additionally shared with
iOS, it would be in the form of a layered component and place
`content/public/browser` dependencies in `components/foo/content/browser`). Note
that `third_party/blink/public` is a renderer process directory except for
`third_party/blink/public/common` which can be used by all processes.

Note that there may also be an `android` subdir, with a Java source code
structure underneath it where the package name is org.chromium.components.foo,
and with subdirs after 'foo' to illustrate process, e.g. 'browser' or
'renderer':

  * `components/foo/android/`{`OWNERS`, `DEPS`}
  * `components/foo/android/java/src/org/chromium/components/foo/browser/`
  * `components/foo/android/javatests/src/org/chromium/components/foo/browser/`

Code in a component should be placed in a namespace corresponding to the name of
the component; e.g. for a component living in `//components/foo`, code in that
component should be in the `foo::` namespace.
