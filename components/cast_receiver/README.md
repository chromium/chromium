# Introduction

This component provides the shared Cast receiver implementation that is used by
various embedders throughout Chromium. It is planned to be used for WebEngine,
Chromecast hardware, and others.

# Usage

The specifics of integrating this component with an existing Chromium embedder
are described below. The canonical implementation of this component can be found
at
[`//chromecast/cast_core`](https://source.chromium.org/chromium/chromium/src/+/main:chromecast/cast_core/).
For specific usages of the below described APIs, see its
[RuntimeServiceImpl](https://source.chromium.org/chromium/chromium/src/+/main:chromecast/cast_core/runtime/browser/runtime_service_impl.h;l=33)
and
[RuntimeApplicationServiceImpl](https://source.chromium.org/chromium/chromium/src/+/main:chromecast/cast_core/runtime/browser/runtime_application_service_impl.h)
classes, which use this component to implement a
[`gRPC`-defined service](https://source.chromium.org/chromium/chromium/src/+/main:third_party/cast_core/public/src/proto/runtime/runtime_service.proto).

## Integration With Existing Code

Integration with an existing Chromium embedder is relateively easy, with only
a small number of integration points required:

### Browser-Side Integration

Browser-side integration has two parts:

#### Permissions Management

The
[`Permissions Manager`](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/permissions_manager.h)
is used to define the
[permissions](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/common/permissions/permission_utils.h;drc=b8524150039182faf7988e9478a9eff89728ac03;l=23)
that can be used by a given application. It is integrated into an existing
Chromium embedder by
[calling into](https://source.chromium.org/chromium/chromium/src/+/main:chromecast/browser/cast_permission_manager.cc;l=104)
the
[PermissionsManager::GetPermissionsStatus()](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/permissions_manager.h;l=37)
function from the embedder's implementation of
[`content::PermissionControllerDelegate::GetPermissionStatus()`](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/permission_controller_delegate.h;l=75).

#### Runtime Hooks

The remaining integration is done by creating an instance of the
[`ContentBrowserClientMixins` class](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/content_browser_client_mixins.h;l=45)
in the `ContentBrowserClient` implementation for this embedder. For instance,
this is
[currently done](https://source.chromium.org/chromium/chromium/src/+/main:chromecast/cast_core/runtime/browser/cast_runtime_content_browser_client.cc;l=54)
in the Cast Core implementation. From there, the `OnWebContentsCreated()` and
`CreateURLLoaderThrottles()` functions must be called from the
`ContentBrowserClient` functions of the same name.

The embedder may additionally call `AddApplicationStateObserver()` or
`AddStreamingResolutionObserver()` to subscribe to state change events for the
runtime.

### Renderer-Side Integration

Renderer side integration is done very similarly to the runtime hooks for the
browser-side integration as described above. Specifically, from the embedder's
`ContentRendererClient` implementation, an instance of
`ContentRendererClientMixins` must be
[created](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/renderer/public/content_renderer_client_mixins.h;l=34)
as is
[currently done](https://source.chromium.org/chromium/chromium/src/+/main:chromecast/renderer/cast_content_renderer_client.cc;l=88)
in the Cast Core implementation. Then, the functions of this calls must all be
called from the appropriate `ContentRendererClient` functions as outlined in the
class's documentation.

## Running Applications

### Lifetime of an Application

Once the above integration is done, applications can be created by first
[creating an instance](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/content_browser_client_mixins.h;l=88)
of `RuntimeApplicationDispatcher` using the
`ContentBrowserClientMixins::CreateApplicationDispatcher()` function, then
calling
[`CreateApplication()`](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/runtime_application_dispatcher.h;l=36)
and providing
[basic information about the application](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/application_config.h)
(such as application id, requested permissions, etc). Note that this requires a
template parameter of a type implementing the
[`EmbedderApplication` interface](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/embedder_application.h;l=29).

After creation, an application will always exist in one of the following
lifetime states, transitioning between them using functions defined in the
[`RuntimeApplication` interface](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/runtime_application.h;l=19):

1. _Created_: In this state, the `RuntimeApplication` object has been created,
but nothing else has been done.
2. _Loaded_: In this state, information pertaining to the application has been
provided (e.g. any platform-specific application info), and is considered to
have started "running", but no content should be displayed on the screen.
3. _Launched_: The application has been displayed on the screen, and the user
may interact with it.
4. _Stopped_: The application is no longer running and should not be displayed.

It is expected that the application will be _loaded_ immediately after being
_created_, and then _launched_ shortly after.

When the application is to be destroyed, this can be done through calling
`RuntimeApplicationDispatcher::DestroyApplication()`.

### Embedder-Specific Application Details

Implementing
[`EmbedderApplication`](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/embedder_application.h;l=29)
is where the majority of the embedder's work is located. Doing so requires the
following:

- Callbacks to inform the embedder's infrastructure of application state changes
(`NotifyApplicationStarted()`, `NotifyApplicationStopped()`, and
`NotifyMediaPlaybackChanged()`).
- Accessors to the application-specific data for displaying its contents
(`GetWebContents()` and `GetAllBindings()`).
- Embedder-specific controls for the underlying application
 (`GetMessagePortService()` and `GetContentWindowControls()`).
- Other optional overloads that may be needed depending on the embedder's
infrastructure.

Implementing this type therefore requires at minimum implementations of the
following two embedder-specific classes:
- `ContentWindowControls`: Used for controlling the UX Window associated with
this application.
- `MessagePortService`: A wrapper around message port functionality, used to
handle communication with services outside of this component.

### Connecting Runtime Applications and Embedder Applications

When
[creating an instance](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/runtime_application_dispatcher.h;l=28)
of `EmbedderApplication` through `RuntimeApplicationDispatcher`, an instance of
[`RuntimeApplication`](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/runtime_application.h;l=19)
is provided, which can be used for control of this application. Specifically:

- Application Lifetime can be controlled through the `Load()`, `Launch()`, and
`Stop()` functions.
- Application State can be controlled with the `SetMediaBlocking()`,
`SetVisibility()`, `SetTouchInputEnabled()`, and `SetUrlRewriteRules()`
functions.

A pointer to this instance of `EmbedderApplication` will also be provided to the
`RuntimeApplicaiton`, which will use the callbacks and controls as described
previously throughout the application's lifetime.

# Architecture

## Code Structure

The top-level object with which the embedder will interact is the
`ContentBrowserClientMixins` class which will be used to
[create](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/content_browser_client_mixins.h;l=88)
a `RuntimeApplicationDispatcher` instance. That instance will
[create](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/runtime_application_dispatcher.h;l=36)
`RuntimeApplications` instances, either `StreamingRuntimeApplication` or
`WebRuntimeApplication` instances, and then
[wrap them](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/runtime_application_dispatcher_impl.h;l=86;drc=790df4d5983e38ad1d1d00fbc10ef941070eed24)
in an `EmbedderApplication` instance using a factory provided by the embedder.
The `EmbedderApplication` instance will control the `RuntimeApplication` with
the following
[commands](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/runtime_application_dispatcher.h):

*   `Load()`
*   `Launch()`
*   `Stop()`

Additionally, the `RuntimeApplication`
[exposes](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/runtime_application.h;drc=12be03159fe22cd4ef291e9561762531c2589539)
a number of accessors and ways to set properties of the application, such as
enabling or disabling touch input. The `EmbedderApplication`
[exposes](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/embedder_application.h;drc=12be03159fe22cd4ef291e9561762531c2589539)
functions to supply embedder-specific types or commands, such as:

*   `GetAllBindings()`
*   `GetMessagePortService()`
*   `GetContentWindowControls()`
*   `GetStreamingConfigManager()`

Additionally, it exposes functions by which it may be informed of state changes
in the `RuntimeApplication` instance it owns.

![cast_receiver code structure](/docs/images/cast_receiver_code_structure.png "cast_receiver code structure")


## Common Scenarios

In each of the following diagrams, blue boxes are used to represent
embedder-specific infrastructure, while white boxes are part of the component.

This component supports two types of applications: Web Applications and
Streaming Applications. Much of the infrastructure for these two application
types is shared, but the differences are substantial enough that each will be
discussed independently.

### Web Applications

Web Applications are used for hosting the majority of applications for a Cast
receiver. At a high level, the flow for using a Web Application is:

1. [Create](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/content_browser_client_mixins.h;l=45;drc=12be03159fe22cd4ef291e9561762531c2589539)
a new `ContentBrowserClientMixins` instance in the embedder-specific
`ContentBrowserClient` implementation, and then use that instance to
[create](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/content_browser_client_mixins.h;l=88;drc=12be03159fe22cd4ef291e9561762531c2589539)
a `RuntimeApplicationDispatcher`.
2. Use the `RuntimeApplicationDispatcher` to
[create](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/runtime_application_dispatcher.h;l=36;drc=1bcc6d9e4af49c462d3b2bee9f00db757084d262)
a `WebRuntimeApplication` instance, which will then be used to
[create](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/runtime_application_dispatcher_impl.h;l=86;drc=790df4d5983e38ad1d1d00fbc10ef941070eed24) an instance of the embedder-specific `EmbedderApplication` type.
3. Call `Load()` on the `RuntimeApplication` instance, and wait for its
callback.
4. Call `SetUrlRewriteRules()` on the `RuntimeApplication`. This may be called
at any time, but is expected to be called at least once before the `Launch()`
command if such rules are required for the application’s functionality.
5. Call `Launch()` on the `RuntimeApplication` instance, and wait for its
callback. Various `EmbedderApplication` functions will be called to create the
necessary resources.
6. Once the application is no longer needed, close it with `StopApplication()`
and then
[destroy](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/runtime_application_dispatcher.h;l=48;drc=1bcc6d9e4af49c462d3b2bee9f00db757084d262)
the application with the `RuntimeApplicationDispatcher` after the
`StopApplication()`’s callback returns.

![Web Application code flow](/docs/images/cast_receiver_webruntimeapplication_flow.png "WebRuntimeApplication structure")

### Streaming

Streaming Applications are used to support the Cast streaming and remoting
scenarios by making use of the `cast_streaming` component

1. [Create](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/content_browser_client_mixins.h;l=45;drc=12be03159fe22cd4ef291e9561762531c2589539)
a new `ContentBrowserClientMixins` instance in the embedder-specific
`ContentBrowserClient` implementation, and then use that instance to
[create](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/content_browser_client_mixins.h;l=88;drc=12be03159fe22cd4ef291e9561762531c2589539)
a `RuntimeApplicationDispatcher`.
2. Use the `RuntimeApplicationDispatcher` to
[create](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/runtime_application_dispatcher.h;l=36;drc=1bcc6d9e4af49c462d3b2bee9f00db757084d262)
a `StreamingRuntimeApplication` instance, which will then be used to
[create](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/runtime_application_dispatcher_impl.h;l=86;drc=790df4d5983e38ad1d1d00fbc10ef941070eed24)
an instance of the embedder-specific `EmbedderApplication` type.
3. Call `Load()` on the `RuntimeApplication` instance, and wait for its
callback.
4. Call `Launch()` on the `RuntimeApplication` instance, and wait for its
callback. Various `EmbedderApplication` functions will be called to create the
necessary resources, which will be used to start the `cast_streaming` component.
Each of the following is
[expected](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/streaming_controller_base.cc;l=88;drc=790df4d5983e38ad1d1d00fbc10ef941070eed24)
to occur for the streaming session to successfully begin, but the order may
vary:
    1. Calling of `StartPlaybackAsync()`, which will be called as part of the
    `Launch()` command before its callback is called.
    2. The `WebContents` instance associated with this application, as returned
    by `EmbedderApplication::GetWebContents()`,
    [loads](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/streaming_controller_base.cc;l=46)
    the page used for displaying the streaming session.
    3. The configuration to use for streaming is returned by the
    embedder-specific `ConfigurationManager` as
    [provided](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/streaming_config_manager.h;l=25;drc=576992499f3c1488c8f86feafb3a65aee426f784)
    by `EmbedderApplication::GetStreamingConfigManager()`.
5. Once streaming has started and a connection has been formed with the Cast
sender device, a `OnStreamingResolutionChanged()`
[event](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/streaming_resolution_observer.h;l=29)
will be fired.
6. Streaming may be stopped by the embedder as with Web applications.
Alternatively, if the session is
[ended](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/public/receiver_session.h;l=49;drc=7eb26cecf3a3c92e25c68b8ca4f0fc467ea89af7)
by the remote device, a `NotifyApplicationStopped()` event will be fired to the
`EmbedderApplication`, at which point the application should be
[destroyed](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_receiver/browser/public/runtime_application_dispatcher.h;l=48;drc=1bcc6d9e4af49c462d3b2bee9f00db757084d262)
by the `RuntimeApplicationDispatcher`.

![Streaming Application code flow](/docs/images/cast_receiver_streamingruntimeapplication_flow.png "StreamingRuntimeApplication structure")

# Known Issues and Limitations

- TODO(crbug.com/1405480): DRM is not supported.
