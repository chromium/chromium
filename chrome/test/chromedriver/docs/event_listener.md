# DevTools Event Listener

Each `DevToolsClientImpl` object contains a list of event listeners.
These listeners are called when certain DevTools events occur,
and can run additional code in response to those events.
For example, a listener can listen for page navigation events
in order to keep track of navigation status, while another listener can
keep track of opening and closing of JavaScript dialog boxes, and so on.

(See [Chrome Connection](chrome_connection.md) for more information about
`DevToolsClientImpl` object.)

An event listener must be an object that implements the interface class
[`DevToolsEventListener`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/devtools_event_listener.h?q=DevToolsEventListener).
Each listener is called in the following situations:
* When the DevTools Client connects to Chrome.
  ([`ConnectIfNecessary`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/devtools_client_impl.cc?q=DevToolsClientImpl::ConnectIfNecessary)
  -> [`EnsureListenersNotifiedOfConnect`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/devtools_client_impl.cc?q=DevToolsClientImpl::EnsureListenersNotifiedOfConnect)
  -> [`DevToolsEventListener::OnConnected`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/devtools_event_listener.h?q=OnConnected))
* When an event is received from Chrome.
  ([`ProcessEvent`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/devtools_client_impl.cc?q=DevToolsClientImpl::ProcessEvent)
  -> [`EnsureListenersNotifiedOfEvent`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/devtools_client_impl.cc?q=DevToolsClientImpl::EnsureListenersNotifiedOfEvent)
  -> [`DevToolsEventListener::OnEvent`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/devtools_event_listener.h?q=OnEvent))
* When a command response is received from Chrome.
  ([`ProcessCommandResponse`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/devtools_client_impl.cc?q=DevToolsClientImpl::ProcessCommandResponse)
  -> [`EnsureListenersNotifiedOfCommandResponse`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/devtools_client_impl.cc?q=DevToolsClientImpl::EnsureListenersNotifiedOfCommandResponse)
  -> [`DevToolsEventListener::OnCommandSuccess`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/devtools_event_listener.h?q=OnCommandSuccess))

The DevTools Client has three lists to keep track of listeners that still need
to be notified of these three situations:
[`DevToolsClientImpl::unnotified_connect_listeners_`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/devtools_client_impl.h?q=DevToolsClientImpl::unnotified_connect_listeners_),
[`DevToolsClientImpl::unnotified_event_listeners_`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/devtools_client_impl.h?q=DevToolsClientImpl::unnotified_event_listeners_), and
[`DevToolsClientImpl::unnotified_cmd_response_listeners_`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/devtools_client_impl.h?q=DevToolsClientImpl::unnotified_cmd_response_listeners_).
These lists are necessary because while a listener processes one event,
it can trigger the generation of additional events.

The listeners are *not* notified of commands sent from ChromeDriver to DevTools.

## Listener Registration

Each instance of `DevToolsClientImpl` stores a list of listeners in field
[`DevToolsClientImpl::listeners_`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/devtools_client_impl.h?q=DevToolsClientImpl::listeners_).
Listeners can be added by calling
[`DevToolsClientImpl::AddListener`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/devtools_client_impl.cc?q=DevToolsClientImpl::AddListener).
* For the browser-wide DevTools client, this is done by
  [`CreateBrowserwideDevToolsClientAndConnect`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome_launcher.cc?q=CreateBrowserwideDevToolsClientAndConnect)
  shortly after Chrome starts.
* For other DevTools clients, this happens in
  [`ChromeImpl::UpdateWebViews`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/chrome_impl.cc?q=ChromeImpl::UpdateWebViews) and
  [`WebViewImpl` constructor](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/web_view_impl.cc?q=WebViewImpl::WebViewImpl).

Most listeners are added indirectly by `WebViewImpl` constructor.
The constructor instantiates several listener objects,
and the constructors for those listener objects are responsible for calling
`DevToolsClientImpl::AddListener` to add themselves.

## Details of Listeners

ChromeDriver has the following event listeners:
* [`CastTracker`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/cast_tracker.h?q=CastTracker)
* [`ConsoleLogger`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/console_logger.h?q=ConsoleLogger)
* [`DevToolsEventsLogger`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/devtools_events_logger.h?q=DevToolsEventsLogger)
* [`DomTracker`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/dom_tracker.h?q=DomTracker)
* [`DownloadDirectoryOverrideManager`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/download_directory_override_manager.h?q=DownloadDirectoryOverrideManager)
* [`FedCmTracker`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/fedcm_tracker.h?q=FedCmTracker)
* [`FrameTracker`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/frame_tracker.h?q=FrameTracker)
* [`GeolocationOverrideManager`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/geolocation_override_manager.h?q=GeolocationOverrideManager)
* [`HeapSnapshotTaker`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/heap_snapshot_taker.h?q=HeapSnapshotTaker)
* [`JavaScriptDialogManager`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/javascript_dialog_manager.h?q=JavaScriptDialogManager)
* [`MobileEmulationOverrideManager`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/mobile_emulation_override_manager.h?q=MobileEmulationOverrideManager)
* [`NavigationTracker`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/navigation_tracker.h?q=NavigationTracker)
* [`NetworkConditionsOverrideManager`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/network_conditions_override_manager.h?q=NetworkConditionsOverrideManager)
* [`PerformanceLogger`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/performance_logger.h?q=PerformanceLogger)

The listener description below is still incomplete.
More will be added in the future.

### Navigation Tracker

The [`NavigationTracker`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/navigation_tracker.h?q=NavigationTracker)
implements the [wait for navigation to complete](https://www.w3.org/TR/webdriver/#dfn-waiting-for-the-navigation-to-complete)
algorithm defined by the WebDriver standard.
It is used when the
[page loading strategy](https://www.w3.org/TR/webdriver/#dfn-page-loading-strategy)
is [normal](https://www.w3.org/TR/webdriver/#dfn-normal-page-loading-strategy) (the default)
or [eager](https://www.w3.org/TR/webdriver/#dfn-eager-page-loading-strategy).
(When the page loading strategy is
[none](https://www.w3.org/TR/webdriver/#dfn-none-page-loading-strategy),
ChromeDriver uses [`NonBlockingNavigationTracker`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/non_blocking_navigation_tracker.h?q=NonBlockingNavigationTracker),
which is not a DevTools event listener.)

`NavigationTracker` keeps track of the loading state of the current
browsing context (either a window/tab or a frame).
The loading state is represented by
[`enum LoadingState`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/page_load_strategy.h?q=LoadingState),
and can be one of three values: `kUnknown`, `kLoading`, and `kNotLoading`.
`NavigationTracker` moves the loading state between these values based on the
events it receives from Chrome DevTools.
Most ChromeDriver commands wait for the loading state to become `kNotLoading`
before returning.

### FedCM Tracker

The [`FedCmTracker`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/chromedriver/chrome/fedcm_tracker.h?q=FedCmTracker)
listens to FedCM dialog events and stores the parameters for later use.

Since the WebDriver protocol does not support events, we instead provide
accessors that the client can periodically check to detect when a dialog has
been shown.
