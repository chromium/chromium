# About //components

This directory is meant to house features or subsystems that are used in more
than one part of the Chromium codebase.

## Example use cases:

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

## Guidelines for adding a new component

  * You will be added to an OWNERS file under `//components/{your component}`
    and be responsible for maintaining your addition.
  * A `//components/OWNER` must approve of the location of your code.
  * Code must be needed in at least 2 places in Chrome that don't have a "higher
    layered" directory that could facilitate sharing (e.g. `//content/common`,
    `//chrome/utility`, etc.).

## Dependencies of a component

Components **cannot** depend on the higher layers of the Chromium codebase:

  * `//android_webview`
  * `//chrome`
  * `//chromecast`
  * `//headless`
  * `//ios/chrome`
  * `//content/shell`

Components **can** depend on the lower layers of the Chromium codebase:

  * `//base`
  * `//gpu`
  * `//mojo`
  * `//net`
  * `//ui`

Components **can** depend on each other. This must be made explicit in the
`DEPS` file of the component.

Components **can** depend on `//content/public` and `//ipc`. This must be made
explicit in the `DEPS` file of the component. If such a component is used by
Chrome for iOS (which does not use content or IPC), the component will have to
be in the form of a [layered
component](http://www.chromium.org/developers/design-documents/layered-components-design).

`//chrome`, `//ios/chrome`, `//content` and `//ios/web` **can** depend on
individual components. The dependency might have to be made explicit in the
`DEPS` file of the higher layer (e.g. in `//content/browser/DEPS`). Circular
dependencies are not allowed: if `//content` depends on a component, then that
component cannot depend on  `//content/public`, directly or indirectly.

## Structure of a component

As mentioned above, components that depend on `//content/public` or `//ipc`
might have to be in the form of a [layered
component](http://www.chromium.org/developers/design-documents/layered-components-design).

Components that have bits of code that need to live in different processes (e.g.
some code in the browser process, some in the renderer process, etc.) should
separate the code into different subdirectories. Hence for a component named
'foo' you might end up with a structure like the following (assuming that foo is
not used by iOS and thus does not need to be a layered component):

  * `components/foo`          - DEPS, OWNERS, BUILD.gn
  * `components/foo/browser`  - code that needs the browser process
  * `components/foo/common`   - for e.g. Mojo interfaces and such
  * `components/foo/renderer` - code that needs renderer process

These subdirectories should have DEPS files with the relevant restrictions in
place, i.e. only `components/foo/browser` should be allowed to #include from
`content/public/browser`.

Note that there may also be an `android` subdir, with a Java source code
structure underneath it where the package name is org.chromium.components.foo,
and with subdirs after 'foo' to illustrate process, e.g. 'browser' or
'renderer':

  * `components/foo/android/OWNERS`, `DEPS`
  * `components/foo/android/java/src/org/chromium/components/foo/browser/`
  * `components/foo/android/javatests/src/org/chromium/components/foo/browser/`

Code in a component should be placed in a namespace corresponding to the name of
the component; e.g. for a component living in `//components/foo`, code in that
component should be in the `foo::` namespace.

## How does this differ from //base/util?

See the explanation in [//base/util/README.md](https://chromium.googlesource.com/chromium/src/+/HEAD/base/util/README.md#how-does-this-differ-from-components).
