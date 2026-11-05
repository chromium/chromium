# Architecture & Review Rules (`components/browser_actuator/AGENTS.md`)

## 1. Core Ownership & Lifetime Invariants

- `TransportServiceImpl` owns `TransportChannelImpl` (and optional `SessionStreamRecorderFactory`).
- `TransportChannelImpl` owns the shared `MessageStreamClient` (`ProtoStreamClient`), `UpstreamMessageClient`, `TransportHandlerFactoryRegistryImpl`, and `TransportSessionRegistryImpl`.
- `TransportSessionRegistryImpl` owns active `TransportSessionImpl` instances; each session owns its `TransportHandler` instances and holds a `base::WeakPtr<TransportChannel>`.
- `TransportSessionRegistryImpl` tracks recently destroyed session IDs in `closed_session_ids_` (`base::LRUCacheSet`) so late downstream messages cannot resurrect closed sessions via `GetOrCreateSession()`.
- `SessionStreamRecorder` stores events in a `std::vector<Entry>` and must only be instantiated when `TransportServiceConfig::record_sessions` (`kBrowserActuatorInternals`) is enabled; `GetAllSessions()` snapshots must bound `max_events` and `max_message_bytes`.
- Handlers are ephemeral and their lifetime is strictly bound to the active `TransportSession`.

## 2. Layering, DEPS & Cross-Directory Boundaries

- Keep `{public,internal,test_support}/transport/` independent of the rest of `//components/browser_actuator` and feature-specific flags/protos (enforced by `DEPS` and `//components/browser_actuator/internal/transport:isolation_check`).
- All public APIs must reside in the `public/` directory.
- Ensure no circular dependencies exist between sharing infrastructure and the base browser actuator.

## 3. High-Signal Review Checklist

- [Check] Verify that all `GetFactories`, `GetStreamRecorderFactory`, and session lookup calls include `SEQUENCE_CHECKER` validation for the owning UI sequence.
- [Check] Ensure `base::expected<void, ProcessPayloadError>` is used explicitly instead of `auto` for payload results.
- [Check] Confirm that PII (credentials, tokens) is never logged in UMA or `DLOG` statements.
- [Check] Verify that `TransportSessionImpl` guards handler callbacks (`OnNewSession`, `OnMessage`) with `base::WeakPtr<TransportSessionImpl>` in case a handler synchronously destroys the session, and that `ProtoStreamClient` defers `Disconnect()`/`Connect()` via `teardown_deferred_` while `notifying_observers_` is true.
- [Check] Check that all histograms use the `Browser.Actuator.*` prefix (not `BrowserActuator.*`).
