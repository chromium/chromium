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
* `internal/`: Internal service implementations, transport routing logic,
  session state machines, and protocol handlers.
* `test_support/`: Fakes, mocks, and test utilities for unit and browser
  integration testing.

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
