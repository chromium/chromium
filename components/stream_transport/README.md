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
  `//components/stream_transport/public:service`):
  Top-level `KeyedService` owning the profile's `TransportChannel`. Exposes
  `GetChannel()`, convenience session lookups (`CreateSession()`,
  `GetSession()`), and optional diagnostic recording
  (`GetStreamRecorderFactory()`). Kept in a `:service` subtarget separate from
  `:public` so handler implementations do not pull in
  `//components/keyed_service/core`.
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

## Directory Layout

- `public/`: Pure abstract interfaces, `common.h` types, and
  `payload_type_mapping.h`.
- `internal/`: Concrete implementations (`TransportServiceImpl`,
  `TransportChannelImpl`, `TransportSessionImpl`, built-in control and probe
  handlers, `SessionStreamRecorder`, `ProtoStreamClient`, Rust `StreamBody`
  framer, OAuth/resume connection delegates, and `UpstreamMessageClient`). See
  [`internal/README.md`](internal/README.md) for architecture details.
- `test_support/`: GMock implementations (`MockTransportService`,
  `MockTransportChannel`, `MockTransportHandler`, `CallbackTransportHandler`,
  `MockTransportHandlerFactory`) and `test_constants.h` for unit testing
  feature handlers without network I/O.

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
