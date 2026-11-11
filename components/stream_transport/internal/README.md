# Stream Transport Internals (`//components/stream_transport/internal`)

This directory contains the concrete implementation of
`//components/stream_transport`. Outside `//components/stream_transport`, only
the `chrome://browser-actuator-internals` WebUI
(`session_stream_recorder.h`, `transport_message_observer.h`) and the
`//chrome/browser` actuator bridges (`internal/proto/`) include headers from
`internal/`. `TransportServiceImpl` is instantiated via
`CreateTransportService()` in `//components/stream_transport/public:factory`.

## Wire Format (`alt=proto`)

The downstream stream is a OnePlatform server-streaming RPC fetched with
`alt=proto` (`Accept: application/x-protobuf`). The HTTP response body is a
single never-ending serialized `google.rpc.StreamBody` protobuf:

```proto
message StreamBody {
  repeated bytes message = 1;    // One per streamed response message.
  google.rpc.Status status = 2;  // At most once, at the end of the RPC.
  repeated bytes noop = 15;      // Keep-alive padding.
}
```

Each complete top-level length-delimited field is one delivery — there is no
other chunking layer. Messages arrive as serialized protos
(`WatchSessionsResponse`), `noop` fields prove the connection is alive (feeding
the stall watchdog), and `status` ends the RPC (no auto-reconnect). OnePlatform
can also encode this RPC as SSE (`alt=sse`), which `MessageStreamClient` could
support as a second implementation; `alt=proto` is used here because the
frontends JSON-encode protos in SSE `data:` lines, which is a poor fit for
binary payloads.

## Architecture Overview

```text
+-----------------------------------------------------------------------+
|                        TransportServiceImpl                           |
|  - Owns TransportChannelImpl (injected with UpstreamMessageClient &   |
|    StreamClientFactory)                                               |
|  - Optionally owns SessionStreamRecorderFactory (for WebUI debug)     |
|  - Observes signin::IdentityManager to reset state on account change  |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
|                        TransportChannelImpl                           |
|  - Owns MessageStreamClient (ProtoStreamClient),                      |
|    UpstreamMessageClient, TransportSessionRegistryImpl, and           |
|    TransportHandlerFactoryRegistryImpl                                |
|  - Registers built-in ControlTransportHandlerFactory &                |
|    ProbeTransportHandlerFactory                                       |
|  - Connects/reconnects/disconnects ProtoStreamClient as sessions      |
|    are registered or removed                                          |
|  - Parses WatchSessionsResponse and routes ActuatorDownstreamMessage  |
|    envelopes to the target TransportSessionImpl                       |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
|                        TransportSessionImpl                           |
|  - Orders downstream messages by sequence_number via                  |
|    DownstreamMessageSequencer (0 = optional FCM wake, 1+ = stream)    |
|  - Validates google.protobuf.Any.type_url against ExpectedTypeUrl()   |
|  - Lazily instantiates per-session TransportHandlers from registered  |
|    factories on first message receipt for each PayloadType            |
|  - Attaches responding_to_sequence_number on control/probe replies    |
|    and assigns monotonically increasing client_sequence_number        |
+-----------------------------------------------------------------------+
             |                                         |
             v (Downstream)                            v (Upstream)
+-----------------------------------+   +-------------------------------+
| ProtoStreamClient                 |   | UpstreamMessageClient         |
|  - StreamConnectionDelegate chain |   |  - OAuth token acquisition    |
|    (OAuth -> ResumeBody)          |   |  - Unary HTTP POST per        |
|  - RustStreamFramer (StreamBody   |   |    SendSessionMessageRequest  |
|    chunked protobuf parser)       |   |  - Bounded in-flight queue    |
|  - Stall watchdog & backoff       |   +-------------------------------+
+-----------------------------------+
```

## Key Subsystems

### 1. Session Lifecycle & Sequencing
- **`TransportSessionRegistryImpl`** (`transport_session_registry_impl.{h,cc}`):
  Owns active `TransportSessionImpl` instances keyed by `session_id` (up to
  `kMaxSessionIdLength` bytes), evicts idle sessions, and notifies `Observer`s
  (`TransportChannelImpl` and, when recording is enabled,
  `SessionStreamRecorderFactory`) when sessions are registered or removed.
- **`DownstreamMessageSequencer`** (`downstream_message_sequencer.{h,cc}`):
  Orders per-session downstream messages by `sequence_number`. In-band stream
  messages start at `1`, while `sequence_number == 0` is reserved for the
  optional out-of-band FCM wake message (`last_seen_sequence_number()` starts at
  `-1`). Early out-of-order messages (`> last_seen + 1`) are buffered up to
  `kDefaultMaxBufferedMessages` (`16`) until contiguous; duplicates
  (`<= last_seen`) are dropped; negative or `INT64_MAX` values fail with
  `Error::kInvalidSequenceNumber`, and buffer overflow returns
  `Error::kBufferFull`. Terminating control commands (`kCloseSession`,
  `kCloseChannel`) flush and bypass reorder buffering so a missing earlier
  sequence number cannot stall teardown.
- **Reply Correlation (`dispatching_sequence_number_`)**:
  While `TransportSessionImpl` is dispatching a downstream message with sequence
  number $N$ (`0` on the out-of-band `OnMessage()` path, or `sequence_number` in
  `ProcessDownstreamMessage()`), `dispatching_sequence_number_` is set to $N$ so
  control and probe replies (`PayloadType::kControl`, `PayloadType::kProbe`)
  populate `responding_to_sequence_number = N` on the outgoing
  `ActuatorUpstreamMessage`.

### 2. Built-in Handlers
- **`ControlTransportHandler`** (`control_transport_handler.{h,cc}`):
  Handles `PayloadType::kControl` (`browser_actuator::ControlCommand`):
  - `kStartSession`: replies upstream with `StartSessionAck`.
  - `kCloseSession`: invokes `CloseSessionCallback`
    (`TransportSessionRegistryImpl::DestroySession`) to destroy the session.
  - `kCloseChannel`: invokes `CloseChannelCallback`
    (`TransportChannelImpl::Disconnect`) to close the downstream stream
    connection.
- **`ProbeTransportHandler` & `probe_payload`**
  (`probe_transport_handler.{h,cc}`, `probe_payload.{h,cc}`):
  Handles `PayloadType::kProbe` (`browser_actuator::TransportProbe`). Validates
  `ProbeRequest` against size and per-session in-flight delayed response budgets
  (`kMaxProbeResponses`, `kMaxTotalProbeResponseBytes`), then emits zero or more
  immediate or delayed `ProbeResponse` messages (`GenerateProbeResponsePayload`)
  as specified by `ProbeRequest.response_specs`.

### 3. Wire-Level Downstream Stream (`proto_stream/`)
`internal/proto_stream/` is a strictly isolated sub-layer (enforced by
`internal/proto_stream/DEPS` and `internal/proto_stream:isolation_check`) with
visibility restricted to `//components/stream_transport/public/proto_stream:factory`.
Even `TransportChannelImpl` and `TransportServiceImpl` in `internal/` consume
it purely through `//components/stream_transport/public/proto_stream`
(`internal/DEPS` forbids `-components/stream_transport/internal/proto_stream`).

- **`MessageStreamClient`** (`../public/proto_stream/message_stream_client.h`):
  Wire-format-agnostic public interface for subscribing to a server-to-client
  stream of serialized protobuf messages (`Observer::OnStreamMessage`,
  `OnStreamStatus`, `OnStreamConnectionStateChange`).
- **`ProtoStreamClient`** (`proto_stream/proto_stream_client.{h,cc}`,
  exposed publicly via `CreateProtoStreamClient()` in
  `../public/proto_stream/stream_client.h`):
  Drives a `network::SimpleURLLoader` streaming download (`DownloadAsStream`),
  feeds raw chunks into a `StreamFramer`, notifies the
  `StreamConnectionDelegate` before observers, arms a stall watchdog timer, and
  handles exponential backoff reconnects on transient network/HTTP errors or
  non-terminal stream ends.
- **`RustStreamFramer` & `stream_body_parser.rs`**
  (`proto_stream/rust_stream_framer.{h,cc}`,
  `proto_stream/rust/stream_body_parser.rs`, exposed publicly via
  `CreateRustStreamFramer()` in `../public/proto_stream/stream_client.h`):
  To satisfy Chromium's Rule of 2 when parsing untrusted network bytes before
  handing individual message slices to C++ `MessageLite::ParseFromString()`, the
  outer `google.rpc.StreamBody` framing (`field 1` repeated `bytes message`,
  `field 2` `google.rpc.Status status`, `field 15` repeated `bytes noop`) is
  decoded incrementally in `#![forbid(unsafe_code)]` Rust across arbitrary chunk
  boundaries, enforcing a 10-byte varint bound and capping every
  length-delimited field at `MAX_MESSAGE_BYTES` (`10 MiB`).

### 4. Connection Delegates (`proto_stream/`)
- **`StreamConnectionDelegate`**
  (`../public/proto_stream/stream_connection_delegate.{h,cc}`):
  Public policy seam for `ProtoStreamClient` controlling asynchronous request
  preparation (`PrepareRequest`), upload body provision
  (`GetConnectionRequestBody`), message inspection (`OnMessageDispatched`), and
  HTTP failure retry policy (`ShouldRetryOnHttpFailure`).
- **`OAuthStreamConnectionDelegate`**
  (`proto_stream/oauth_stream_connection_delegate.{h,cc}`, exposed publicly via
  `CreateOAuthStreamConnectionDelegate()` in
  `../public/proto_stream/oauth_stream_connection_delegate.h`):
  Fetches an OAuth2 access token from `signin::IdentityManager`
  (`PrimaryAccountAccessTokenFetcher::Mode::kImmediate`), attaches
  `Authorization: Bearer <token>`, and on HTTP `401` invalidates the cached
  token and requests one retry (`ShouldRetryOnHttpFailure` returns `true` once
  per fresh token). Composes an inner delegate so auth and resume-body logic
  remain orthogonal.
- **`ResumeBodyConnectionDelegate`**
  (`proto_stream/resume_body_connection_delegate.{h,cc}`, exposed publicly via
  `CreateResumeBodyConnectionDelegate()` in
  `../public/proto_stream/stream_client.h`):
  Invokes a channel-supplied `BodyProvider` callback on each connection attempt
  to upload a freshly serialized `WatchSessionsRequest` listing every active
  session's ID and `last_seen_sequence_number` (when `> 0`), so reconnects
  resume cleanly without holding duplicate sequence state in the delegate.

### 5. Upstream Delivery (`upstream_message_client/`)
- **`UpstreamMessageClient`** (`upstream_message_client/`):
  Assembles `ActuatorUpstreamMessage` envelopes and sends
  `SendSessionMessageRequest` protos over authenticated unary HTTPS `POST`
  requests via `endpoint_fetcher::EndpointFetcher`, enforcing a bounded
  in-flight request cap and supporting `CancelPendingRequests()` on sign-out or
  primary-account change.

### 6. Diagnostics & WebUI Recording (`session_stream_recorder.*`)
- **`SessionStreamRecorder` & `SessionStreamRecorderFactory`**:
  When `TransportServiceConfig::record_sessions` is enabled (gated by
  `browser_actuator::kBrowserActuatorInternals`), records a bounded per-session
  buffer (`kMaxEntriesPerSession = 256`) of session metadata and downstream/upstream
  envelopes (retaining up to `kMaxRetainedSessions = 100` closed sessions) and
  serializes formatted JSON snapshots (using generated `proto_extras` `ToValue()`
  serializers) for inspection in `chrome://browser-actuator-internals`.

### 7. Wire Protobuf Package (`proto/`)
The `.proto` files in `internal/proto/` (`transport_messages.proto` and
`task_ingress.proto`) intentionally retain `package browser_actuator;` so that
wire `google.protobuf.Any.type_url` strings
(`type.googleapis.com/browser_actuator.*`) remain wire-compatible with deployed
server endpoints.
