# ChromeOS Mojo Services and Allowlisting for IWAs (`chromeos/ash/experiences/isolated_web_app`)

This directory hosts ChromeOS Ash-specific feature enablement, API allowlists,
window targeters, and Mojo services restricted to Isolated Web Apps (IWAs).

## Companion Documentation

- **Blink `window.setShape` Frontend:**
  [`//third_party/blink/renderer/modules/set_shape`](/third_party/blink/renderer/modules/set_shape/README.md)
- **Set Shape Browser Tests:**
  [`//chrome/browser/ash/set_shape`](/chrome/browser/ash/set_shape/README.md)
- **Core IWA Component:**
  [`//components/webapps/isolated_web_apps`](/components/webapps/isolated_web_apps/README.md)

## Key Components

- `CrosIsolatedWebAppEnabler` (`cros_isolated_web_app_enabler.{h,cc}`):
  `content::WebContentsObserver` that enables the Blink `SetShape` runtime
  feature (`blink::RuntimeFeatureStateContext::SetSetShapeEnabled(true)`) during
  `ReadyToCommitNavigation` for `isolated-app://` navigations allowed by
  `CanOriginAccessCrosIwaApi()` or `blink::features::kSetShape`.
- `isolated_web_app_api_allowlist.{h,cc}` (`CanOriginAccessCrosIwaApi`):
  Evaluates whether an `isolated-app://` origin is allowlisted via
  `IwaRuntimeDataProvider::GetSpecialAppPermissionsInfo()` (`allow_set_shape`)
  when `chromeos::features::kCrosIsolatedWebAppSetShapeAllowlist` is enabled.
- `SetShapeServiceImpl` (`set_shape_service_impl.{h,cc}`): Browser-process Mojo
  service (`content::DocumentUserData<SetShapeServiceImpl>`) implementing
  `blink::mojom::SetShapeService`
  (`//third_party/blink/public/mojom/set_shape/set_shape.mojom`). Validates that
  the window is in unframed mode, verifies rectangle bounds and minimum 10x10
  visible area, applies the hit-test mask to the `aura::Window`
  (`layer()->SetAlphaShape`), and installs a `ShapedWindowTargeter`.
- `ShapedWindowTargeter` (`shaped_window_targeter.{h,cc}`): Custom
  `wm::MaskedWindowTargeter` subclass ensuring mouse and touch events outside
  the configured shape regions pass through to underlying windows.
