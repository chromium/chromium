# Chrome Connection

## Overview

ChromeDriver uses several classes to connect to Chrome and control it.
* `Chrome` class represents the Chrome browser controlled by ChromeDriver.
* `DevToolsClient` class represents a connection to Chrome.
* `WebView` class wraps around a `DevToolsClient` to provide some higher level
  methods.

## `Chrome` class

ChromeDriver uses abstract class `Chrome` to represent the Chrome browser that
it controls. Other classes derived from it provide the actual implementations.
Here is its inheritance hierarchy:

* [`Chrome`](../chrome/chrome.h): Abstract base class, defining the API.
  * [`ChromeImpl`](../chrome/chrome_impl.h): Still an abstract class,
    it contains implementation code that is shared by concrete classes.
    * [`ChromeDesktopImpl`](../chrome/chrome_desktop_impl.h):
      Represents a local desktop Chrome browser.
    * [`ChromeAndroidImpl`](../chrome/chrome_android_impl.h):
      Represents an adb connection to a Chrome running on an Android device.
    * [`ChromeRemoteImpl`](../chrome/chrome_remote_impl.h):
      Represents a Chrome browser that is started
      independently from ChromeDriver, and is connected through a TCP port.
      The browser can be running locally or remotely.
  * [`StubChrome`](../chrome/stub_chrome.h): An implementation with all methods
    stubbed out, used for unit tests only.
    * Various test classes.

There is a separate instance of `ChromeImpl` for each ChromeDriver session.
It has several fields related to the connection between
ChromeDriver and the browser:

* `ChromeImpl::devtools_http_client_` is a `DevToolsHttpClient` object.
  It contains the URL necessary to connect to browser DevTools (e.g.,
  http://localhost:12345), but doesn't contain any actual connections.

* `ChromeImpl::devtools_websocket_client_` is a `DevToolsClient` object.
  It encapsulates a browser-wide DevTools connection, used for sending commands
  that apply to the whole browser.
  This instance of `DevToolsClient` is sometimes referred to as the
  "browser-wide `DevToolsClient`".

* `ChromeImpl::web_views_` is a list of `WebView` objects,
  one for each tab active in the browser.
  These objects are used for sending commands that apply to a specific tab.
  `Chrome::GetWebViewById` allows retrieving a `WebView` by its ID.
  (There are additional `WebView` instances that represent frames,
  but `ChromeImpl` is not aware of them.)

## `DevToolsClient` and `WebView`

Abstract class [`DevToolsClient`](../chrome/devtools_client.h) and its
implementation class [`DevToolsClientImpl`](../chrome/devtools_client_impl.h)
handle communication between ChromeDriver and DevTools.
`DevToolsClient` contains methods to connect to Chrome,
send commands to Chrome, and receive responses and events from Chrome.
Changes are rarely needed at this level to implement new features.

The abstract class [`WebView`](../chrome/web_view.h) and its implementation
class [`WebViewImpl`](../chrome/web_view_impl.h) contain higher-level
methods on top of `DevToolsClient`. In addition to wrappers to methods
provided by `DevToolsClient`, it also has higher level methods for
synthetic event dispatching, cookie handling, etc.
The rest of ChromeDriver usually interacts with Chrome through `WebView` class.

There are several types of `DevToolsClient` and `WebView` instances:

* Browser-wide `DevToolsClient`, with no matching `WebView`.
  It has id `"browser"`.

* `DevToolsClient` and `WebView` representing a tab or window.
  Both should have the same id, a 32-digit uppercase hexadecimal number.

* `DevToolsClient` and `WebView` representing an OOPIF (out-of-process iframe,
  i.e., a frame connected to a different renderer process than its parent
  frame). They are created in response to
  [`Target.attachedToTarget` event](https://chromedevtools.github.io/devtools-protocol/tot/Target#event-attachedToTarget) from DevTools.
  Each OOPIF has a `targetId` and a `sessionId` (not to be confused
  with ChromeDriver's session ID), both 32-bit hexadecimal numbers.
  The `DevToolsClient` uses the `sessionId` as its ID, and has a parent
  `DevToolsClient`. The `WebView` uses the `targetId` (same as `frameId`)
  as its ID, and has a parent `WebView`.
  No real connections are made. All communications are forwarded by the parent
  `DevToolsClient` and `WebView`.

Note that in most case, each instance of `DevToolsClient` is wrapped by a
`WebView`. The browser-wide `DevToolsClient` is the only instance not wrapped.

From the point of view of the client application, each browser tab or window is
represented by a window handle, a string formed by concatenation `"CDwindow-"`
with the ID of the `DevToolsClient` and `WebView` representing the tab/window.
Each session has a tab that is currently active. The `WebView` connected to that
tab can be retrieved with `Session::GetTargetWindow`.
