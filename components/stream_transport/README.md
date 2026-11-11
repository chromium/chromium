# Stream Transport (`//components/stream_transport`)

`//components/stream_transport` provides a multiplexed, session-oriented
bidirectional protobuf transport over OnePlatform HTTP `alt=proto`
(`google.rpc.StreamBody`) and unary upstream HTTP `POST` requests.

It is an independent component with zero dependencies on feature consumers
(enforced by `DEPS` and `//components/stream_transport/internal:isolation_check`).
In `//chrome/browser`, it is hosted as a per-`Profile` `KeyedService` via
`stream_transport::TransportServiceFactory` in
`//chrome/browser/stream_transport`.

## Core Concepts & Public API (`public/`)

All public interfaces live in `//components/stream_transport/public/` under
`namespace stream_transport`:

- **`TransportService`** (`public/transport_service.h`, target
  `//components/stream_transport/public:service`) &
  **`CreateTransportService()`** (`public/transport_service_factory_helper.h`,
  target `//components/stream_transport/public:factory`):
  Top-level `KeyedService` owning the profile's `TransportChannel`. Exposes
  `GetChannel()`, convenience session lookups (`CreateSession()`,
  `GetSession()`), and optional diagnostic recording
  (`GetStreamRecorderFactory()`). Kept in a `:service` subtarget separate from
  `:public` so handler implementations do not pull in
  `//components/keyed_service/core`, while `:factory` provides
  `CreateTransportService()` so embedder `KeyedService` factories do not reach
  into `internal/`.
- **`TransportChannel`** (`public/transport_channel.h`):
  Manages the long-lived downstream stream and upstream request pipe. Owns the
  `TransportSessionRegistry` and `TransportHandlerFactoryRegistry`. The
  downstream connection opens on demand when the first session is created and
  closes when the last active session is destroyed.
- **`TransportSession`** (`public/transport_session.h`) &
  **`TransportSessionRegistry`** (`public/transport_session_registry.h`):
  Represents a single server-initiated logical session identified by an opaque
  `session_id` (closed via `TransportSession::Close()` or
  `TransportSessionRegistry::DestroySession()`). Demultiplexes incoming
  downstream payloads by `PayloadType` to registered per-session
  `TransportHandler`s and serializes outgoing upstream
  `google::protobuf::MessageLite` payloads via `SendUpstreamMessage()`.
- **`TransportHandler`** (`public/transport_handler.h`),
  **`TransportHandlerFactory`** (`public/transport_handler_factory.h`), &
  **`TransportHandlerFactoryRegistry`**
  (`public/transport_handler_factory_registry.h`):
  Extension point for feature consumers. A feature registers a
  `TransportHandlerFactory` (identified by a unique `FactoryId` and declaring
  its supported `PayloadType`s) with the channel's
  `TransportHandlerFactoryRegistry`. When a session first receives a message
  for a supported `PayloadType`, `OnNewSession()` lazily instantiates a
  `TransportHandler` bound to that `TransportSession`.
- **`PayloadType` & Wire Mapping** (`public/common.h`,
  `public/payload_type_mapping.h`):
  Strongly typed C++ enum (`kUnspecified`, `kControl`,
  `kExperimentalTriggering`, `kActuation`, `kTaskIngress`, `kProbe`) decoupled
  from wire protobuf headers. Consumers that translate directly from wire enums
  or inspect `google.protobuf.Any` `type_url` strings depend on
  `//components/stream_transport/public:payload_type_mapping`.
- **Low-Level `alt=proto` Stream Layer (`public/proto_stream/`)**:
  A standalone, reusable OnePlatform HTTP `alt=proto` (`google.rpc.StreamBody`)
  downstream stream layer with zero dependencies on `TransportService`,
  `TransportChannel`, session sequencing, or wire payload protos (enforced by
  `public/proto_stream/DEPS`, `internal/proto_stream/DEPS`, and
  `//components/stream_transport/internal/proto_stream:isolation_check`).
  Layer 2 (`//components/stream_transport/internal`) consumes this layer strictly
  through its public API:
  - `MessageStreamClient` (`public/proto_stream/message_stream_client.h`, target
    `//components/stream_transport/public/proto_stream`): abstract interface for
    connecting, disconnecting, and observing framed protobuf messages
    (`OnStreamMessage`), terminal RPC status (`OnStreamStatus`), and connection
    state transitions (`OnStreamConnectionStateChange`).
  - `StreamConnectionDelegate` & `DefaultStreamConnectionDelegate`
    (`public/proto_stream/stream_connection_delegate.h`, target
    `//components/stream_transport/public/proto_stream`): policy seam for
    asynchronous request decoration (`PrepareRequest`), connection `POST` body
    upload (`GetConnectionRequestBody`), and HTTP error retry decisions
    (`ShouldRetryOnHttpFailure`).
  - `CreateProtoStreamClient()`, `CreateRustStreamFramer()`,
    `CreateResumeBodyConnectionDelegate()`, &
    `CreateOAuthStreamConnectionDelegate()`
    (`public/proto_stream/stream_client.h` and
    `public/proto_stream/oauth_stream_connection_delegate.h`, target
    `//components/stream_transport/public/proto_stream:factory`): factory
    functions for constructing the `StreamBody` client (configured via
    `ProtoStreamClientConfig`), the memory-safe Rust `StreamBody` framer, the
    resume-body delegate, and primary-account GAIA OAuth2 bearer-token
    authentication without exposing `internal/proto_stream/` headers.

## Directory Layout

- `public/proto_stream/`: Public interfaces (`MessageStreamClient`,
  `StreamConnectionDelegate`, `StreamFramer` in `:proto_stream`) and factory
  functions (`stream_client.h`, `oauth_stream_connection_delegate.h` in
  `:factory`) for the low-level OnePlatform `alt=proto` stream layer.
- `internal/proto_stream/`: Concrete implementation of the `alt=proto` stream
  layer (`ProtoStreamClient`, `RustStreamFramer`, `stream_body_parser.rs`,
  `ResumeBodyConnectionDelegate`, and `OAuthStreamConnectionDelegate`). Visibility
  is restricted to `//components/stream_transport/public/proto_stream:factory`.
- `test_support/proto_stream/`: GMock implementations (`MockMessageStreamClient`,
  `MockMessageStreamObserver`, `MockStreamConnectionDelegate`,
  `MockStreamFramer`) and `wait_for.h` for testing `proto_stream` consumers.
- `public/`: Public interfaces for the session-multiplexed transport layer
  (`TransportService`, `TransportChannel`, `TransportSession`,
  `TransportHandler`), `CreateTransportService()` (in `:factory`), `common.h`
  types, and `payload_type_mapping.h`.
- `internal/`: Concrete implementations of the session-multiplexed transport
  layer (`TransportServiceImpl`, `TransportChannelImpl`, `TransportSessionImpl`,
  built-in control and probe handlers, `SessionStreamRecorder`, and
  `UpstreamMessageClient`). Consumes the downstream stream layer exclusively via
  `//components/stream_transport/public/proto_stream`. See
  [`internal/README.md`](internal/README.md) for architecture details.
- `test_support/`: GMock implementations (`MockTransportService`,
  `MockTransportChannel`, `MockTransportHandler`, `CallbackTransportHandler`,
  `MockTransportHandlerFactory`) and `test_constants.h` for unit testing feature
  handlers without network I/O.

## Integrating a Feature Consumer

1. Depend on `"//components/stream_transport/public"` in handler targets, and on
   `"//components/stream_transport/public:service"` plus
   `"//chrome/browser/stream_transport"` in `//chrome/browser` service/factory
   wiring.
2. Implement `stream_transport::TransportHandler` to handle incoming
   `OnMessage(PayloadType, std::string_view)` payloads and call
   `SendUpstreamMessage(PayloadType, message)` to reply.
3. Implement `stream_transport::TransportHandlerFactory` returning your
   `FactoryId` and `GetSupportedPayloadTypes()`.
4. Register the factory on the profile's `TransportService`:
   ```cpp
   if (auto* transport =
           stream_transport::TransportServiceFactory::GetForProfile(profile);
       transport && transport->GetChannel()) {
     transport->GetChannel()->GetHandlerFactoryRegistry()->RegisterFactory(
         handler_factory_.get());
   }
   ```
   Remember to call `UnregisterFactory()` during `KeyedService::Shutdown()` and
   declare `DependsOn(stream_transport::TransportServiceFactory::GetInstance())`
   in your `ProfileKeyedServiceFactory`.

## Building & Testing

```sh
tools/autotest.py -C out/Default components/stream_transport
tools/autotest.py -C out/Default chrome/browser/stream_transport
```
