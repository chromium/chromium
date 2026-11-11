# Architecture & Review Rules (`components/browser_actuator/AGENTS.md`)

## 1. Architecture & Related Components

- The streaming transport layer lives in `//components/stream_transport` and
  `//chrome/browser/stream_transport` (see
  `//components/stream_transport/AGENTS.md` and
  `//components/stream_transport/README.md` for transport ownership invariants
  and review checks).
- `BrowserActuatorServiceImpl` implements `BrowserActuatorService` (`public/`),
  and `colocation/` provides device proximity proof generation
  (`ColocationProofBuilder`, `ColocationTimeSource`).

## 2. Layering, DEPS & Cross-Directory Boundaries

- All public APIs must reside in the `public/` directory.
- Keep `public/` minimal and restrict `internal/` visibility to `:unit_tests`
  and `//chrome/browser/browser_actuator:factory`.
- Ensure no circular dependencies exist between sharing infrastructure and the
  base browser actuator.

## 3. High-Signal Review Checklist

- [Check] Verify that `BrowserActuatorServiceImpl` methods include
  `SEQUENCE_CHECKER` validation for the owning UI sequence.
- [Check] Confirm that PII (credentials, tokens) is never logged in UMA or
  `DLOG` statements.
- [Check] Check that all histograms use the `Browser.Actuator.*` prefix (not
  `BrowserActuator.*`).

## 4. Build & Test

```sh
tools/autotest.py -C out/Default components/browser_actuator
tools/autotest.py -C out/Default chrome/browser/browser_actuator
```
