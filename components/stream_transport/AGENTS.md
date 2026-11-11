# Architecture & Review Rules (`components/stream_transport/AGENTS.md`)

## 1. Core Ownership & Lifetime Invariants

- `TransportServiceImpl` owns `TransportChannelImpl` (and optional
  `SessionStreamRecorderFactory`).
- `TransportChannelImpl` owns the shared `MessageStreamClient`
  (`ProtoStreamClient`), `UpstreamMessageClient`,
  `TransportHandlerFactoryRegistryImpl`, and `TransportSessionRegistryImpl`.
- `TransportSessionRegistryImpl` owns active `TransportSessionImpl` instances;
  each session owns its `TransportHandler` instances and holds a
  `base::WeakPtr<TransportChannel>`. Sessions are created explicitly via
  `CreateSession()` and destroyed via `DestroySession()`, so late downstream
  messages for unknown or closed sessions are ignored by `GetSessionImpl()`.
- `SessionStreamRecorder` stores events in a bounded
  `base::circular_deque<Entry>` (`kMaxEntriesPerSession = 256`) and must only be
  instantiated when `TransportServiceConfig::record_sessions`
  (`kBrowserActuatorInternals`) is enabled; `SessionStreamRecorderFactory`
  retains up to `kMaxRetainedSessions = 100` closed sessions and
  `GetAllSessions()` snapshots must bound `max_events` and `max_message_bytes`.
- Handlers are ephemeral and their lifetime is strictly bound to the active
  `TransportSession`.

## 2. Layering, DEPS & Cross-Directory Boundaries

- **Strict Layering**: `//components/stream_transport` must never depend on
  `//components/browser_actuator` or feature-specific components (enforced by
  `DEPS` and `//components/stream_transport/internal:isolation_check`). The
  only exception is `//components/sharing_message/proto` for shared wire proto
  definitions. All public APIs must reside in `public/`.
- **Namespace vs. Wire Proto Package**: C++ classes and functions live in
  `namespace stream_transport` (and Rust cxx bridge in
  `stream_transport::stream_bridge`). Protobuf definitions in `internal/proto/`
  remain in `package browser_actuator;` to preserve wire
  `google.protobuf.Any.type_url` compatibility.
- **Rule of 2**: Wire framing of chunked `StreamBody` responses lives in Rust
  (`internal/proto_stream_client/rust/stream_body_parser.rs`). Keep unsafe or
  manual byte-level framing out of C++.

## 3. High-Signal Review Checklist

- [Check] Verify that all `GetFactories`, `GetStreamRecorderFactory`, and
  session lookup calls include `SEQUENCE_CHECKER` validation for the owning UI
  sequence.
- [Check] Ensure `base::expected<void, ProcessPayloadError>` is used explicitly
  instead of `auto` for payload results.
- [Check] Confirm that PII (credentials, tokens) is never logged in UMA or
  `DLOG` statements.
- [Check] Verify that `TransportSessionImpl` guards handler callbacks
  (`OnNewSession`, `OnMessage`) with `base::WeakPtr<TransportSessionImpl>` in
  case a handler synchronously destroys the session, and that
  `ProtoStreamClient` defers `Disconnect()`/`Connect()` via `teardown_deferred_`
  while `notifying_observers_` is true.
- [Check] Check that all histograms use the `Browser.Actuator.*` prefix (not
  `BrowserActuator.*`).

## 4. Build & Test

```sh
gn gen out/Default --check
python3 buildtools/checkdeps/checkdeps.py components/stream_transport
tools/autotest.py -C out/Default components/stream_transport
tools/autotest.py -C out/Default chrome/browser/stream_transport
```
