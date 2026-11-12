# Browser Actuator

`//components/browser_actuator` is the cross-platform home for the Browser
Actuator service, which lets a remote backend drive browser actions on the
user's behalf. The `//chrome` integration (`KeyedService` factory and
`chrome://browser-actuator-internals` WebUI) lives in
`//chrome/browser/browser_actuator`.

## Directory Structure

- `public/`: Public header interface (`BrowserActuatorService`) and feature
  flags (`features.h`). Embedders and consumers outside this component should
  only depend on `public/`.
- `internal/`: Concrete implementation (`BrowserActuatorServiceImpl`) and its
  unit tests. Visibility is restricted to this component and the `//chrome`
  service factory.
- `internal/proto/`: Wire protobufs (`task_ingress.proto`). Intentionally
  retains `package browser_actuator;` so wire `google.protobuf.Any.type_url`
  strings (`type.googleapis.com/browser_actuator.*`) remain wire-compatible
  with deployed server endpoints.
- `colocation/`: Colocation proof generation (`ColocationProofBuilder`,
  `ColocationTimeSource`) for proving device proximity.
- `test_support/`: Mock implementation (`MockBrowserActuatorService`) for unit
  and integration testing.

## Stream Transport (`//components/stream_transport`)

The multiplexed, session-oriented bidirectional streaming transport layer lives
in [`//components/stream_transport`](../stream_transport/README.md) (with its
profile `KeyedService` factory at
`//chrome/browser/stream_transport:stream_transport`).

Feature consumers that only need the streaming transport depend on
`//components/stream_transport/public` (and
`//components/stream_transport/public:service` plus
`stream_transport::TransportServiceFactory` in `//chrome/browser`) directly
without depending on `BrowserActuatorService`.
