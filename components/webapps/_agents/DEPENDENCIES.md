# Dependencies & Architectural Boundaries: components/webapps

**Parent:** [AGENTS.md](../AGENTS.md)

## Canonical Layering Documentation

- **Chromium Design Principles:** `/docs/chrome_browser_design_principles.md`
- **DEPS Rules & Verification:** `/docs/dependencies.md`

## Architectural Layer Boundaries (Top to Bottom)

Higher layers may depend on lower layers; lower layers must **never** depend on
higher layers:

1. **Layer 5 (App Service - ChromeOS Classic):**
   `chrome/browser/apps/app_service/` (publishes & consumes web apps)
2. **Layer 4 (Embedders):**
   - **Desktop:** `chrome/browser/web_applications/`,
     `chrome/browser/ui/web_applications/`
   - **Android:** `chrome/android/` (Java UI), `chrome/browser/android/webapk/`
     (Native C++)
3. **Layer 3 (Shared Components):** `components/webapps/browser/` (Core),
   `components/webapps/browser/android/`, `components/webapps/common/`,
   `components/webapps/renderer/`, `components/webapps/isolated_web_apps/`
4. **Layer 2 (Content Layer):** `content/browser/manifest/`, `content/public/`
5. **Layer 1 (Blink Layer):** `third_party/blink/public/common/manifest/`,
   `third_party/blink/renderer/`

## Desktop Layering: App Service & Extensions

1. **App Service Layering & Decoupling
   ([crbug.com/523338828](https://crbug.com/523338828)):**
   - **Target Architecture:** App Service sits *above* Web Apps as a downstream
     consumer/publisher (`chrome/browser/apps/app_service/publishers/`)
     observing web app state. `WebAppProvider` must not call into App Service.
   - **Legacy Inversions Under Decoupling:** Web Apps historically depends on
     primitives in `components/services/app_service/public/cpp/`.
     [crbug.com/523338828](https://crbug.com/523338828) tracks uncoupling this.
   - **Strict Invariant:** Do **NOT** add new dependencies on
     `//components/services/app_service` or `apps::` types in
     `web_applications/` or `components/webapps/`. Always define or use
     `webapps::` types.
2. **Extensions Decoupling:**
   - Web Apps is decoupled from Extensions. All interactions with the extension
     system (e.g. for legacy preinstalled app migration) must go through the
     fakeable
     [`ExtensionsManager`](/chrome/browser/web_applications/extensions_manager.h)
     abstraction. Never introduce direct `extensions/` dependencies into Web
     Apps.

## Android Layering

1. **Java ➔ Native ➔ Component Flow:**
   - Java Activity lifecycles (`chrome/android/.../webapps`) call native
     services (`chrome/browser/android/webapk/`) via JNI.
   - Native services delegate shared logic down to
     `components/webapps/browser/android/`.
2. **Shared Components Delegate via `WebappsClient`:**
   - `components/webapps/` must never include `+chrome/` headers. It calls up to
     Android or Desktop embedders exclusively via the abstract
     [`WebappsClient`](../browser/webapps_client.h) interface.
