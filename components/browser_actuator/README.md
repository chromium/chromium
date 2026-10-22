# Browser Actuator

`//components/browser_actuator` is a layered Chromium component that provides
the foundational architecture for service-oriented browser actuation.

## Component Organization

This is a [layered component](https://www.chromium.org/developers/design-documents/layered-components-design/)
designed for cross-platform reuse, and it also cleanly separates public APIs
and internal implementations.

* `public/`: Public API headers and feature flags defining the service
  interface (`BrowserActuatorService`), allowing embedders (e.g., `//chrome`,
  `//ios`) to consume the service without depending on internal implementation
  details.
* `internal/`: The service implementation (`BrowserActuatorServiceImpl`).
* `test_support/`: Fakes, mocks, and test utilities for unit and browser
  integration testing.

### Transport

The bi-directional transport between Chrome and the server lives in the
`transport/` subdirectories and does not depend on anything else in this
component, so other features can use it on its own:

* `public/transport/`: interfaces for features and embedders
  (`TransportService`, `TransportChannel`, `TransportSession`,
  `TransportHandler`, `TransportHandlerFactory`, their registries, and
  `PayloadType`).
* `internal/transport/`: `TransportServiceImpl`, the channel, session and
  registry implementations, control commands, the session stream recorder,
  the transport protos, and the wire layer (see `internal/transport/README.md`).
* `test_support/transport/`: mocks for the transport interfaces.

`TransportService` is a per-profile KeyedService of its own, created by
`//chrome/browser/browser_actuator/transport:TransportServiceFactory`. The
factory decides the endpoints and whether sessions are recorded, so the
transport reads none of its clients' features. Features that use the transport
depend on `TransportServiceFactory` directly rather than on
`BrowserActuatorService`.

`DEPS` files forbid transport code from including the rest of this component,
and `//components/browser_actuator/internal/transport:isolation_check` fails
`gn gen` if a transport target depends on it.

---

## Building & Testing

When building and testing, you can use the following commands:

### Build
```zsh
autoninja -C out/Default components/browser_actuator
```

### Run Unit Tests
To run unit tests for this component, use `autotest.py`:
```zsh
tools/autotest.py -C out/Default components/browser_actuator
```
