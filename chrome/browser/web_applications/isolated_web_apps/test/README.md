# Isolated Web App Browser Test Support (`chrome/browser/web_applications/isolated_web_apps/test`)

This directory provides hermetic test builders, fakes, and utilities for testing
Isolated Web Apps in `unit_tests` and `browser_tests`.

## Key Test Utilities

- `IsolatedWebAppBuilder`, `ManifestBuilder`, `BundledIsolatedWebApp`, &
  `ScopedProxyIsolatedWebApp` (`isolated_web_app_builder.{h,cc}`): Fluent
  builders for constructing on-disk `.swbn` bundles or running dev-mode proxy
  servers with custom manifests, headers, resources, and Ed25519/ECDSA
  signatures.
- `IsolatedWebAppTest` (`isolated_web_app_test.{h,cc}`): Base unit test fixture
  wiring up `BrowserTaskEnvironment`, `TestingProfile`, `FakeWebAppProvider`,
  `IwaTestServerConfigurator`, and `InProcessDataDecoder`.
- `IsolatedWebAppTestUpdateServer`
  (`isolated_web_app_test_update_server.{h,cc}`) & `IwaTestServerConfigurator`
  (`iwa_test_server_configurator.{h,cc}`): Local HTTP and `TestURLLoaderFactory`
  servers serving dynamic `UpdateManifest` JSON and `.swbn` bundles for update
  and policy tests.
- `FakeIwaRuntimeDataProviderMixin` (`fake_iwa_runtime_data_provider_mixin.h`) &
  `IwaRuntimeDataProviderMixin` (`iwa_runtime_data_provider_mixin.{h,cc}`):
  Browser test mixins for configuring fake key rotations, allowlists,
  blocklists, and special permissions.
- `PolicyGenerator` (`policy_generator.{h,cc}`), `policy_test_utils.{h,cc}`, &
  `key_distribution/`: Helpers for configuring force-install policies and key
  distribution data in tests.
