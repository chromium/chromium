# XR Browser Tests

[TOC]

## Introduction

This documentation concerns `xr_browser_test.h`, `xr_browser_test.cc`, and files
that use them or their subclasses.

These files port the framework used by XR instrumentation tests (located in
[`//chrome/android/javatests/src/org/chromium/chrome/browser/vr/`][vr android dir]
and documented in
`//chrome/android/javatests/src/org/chromium/chrome/browser/vr/*.md`) for
use in browser tests in order to test XR features on desktop platforms and Android.

[vr android dir]: https://chromium.googlesource.com/chromium/src/+/main/chrome/android/javatests/src/org/chromium/chrome/browser/vr

This is pretty much a direct port, with the same JavaScript/HTML files being
used for both and the Java/C++ code being functionally equivalent to each other,
so the instrumentation tests' documentation on writing tests using the framework
is applicable here, too. As such, this documentation covers the test runner workflows,
input simulation, and the underlying mock architecture.

## Compiling And Running

### Windows

The tests are compiled in the `xr_browser_tests` target. This is a combination
of the `xr_browser_tests_binary` target, which is the actual test binary, and the
`xr_browser_tests_runner` target, which is a wrapper script that ensures special
setup (such as deploying the mock OpenXR runtime active runtime JSON) is completed
before running the tests.

Once compiled, the tests can be run using the following command line:

```bash
run_xr_browser_tests.py --enable-gpu --test-launcher-jobs=1 --enable-pixel-output-in-tests
```

Additional options such as test filtering can be found by running
`xr_browser_tests.exe --help` and `xr_browser_tests.exe --gtest_help`.

Because the runner is a Python wrapper script, you may need to prepend
`python` to the front of the command on Windows if Python file association is
not set up on your machine.

### Android

On Android, the tests are built and run via the `android_browsertests` target.
Note that due to the deployment of the OpenXR mock trampoline shared library
(`libopenxr_mock.so`) and writing a JSON file to
`'/product/etc/openxr/1/active_runtime.json'`, tests must be run on a rooted
physical device or an Android emulator. Because this is a large target, it is
recommended to append `--gtest_filter=*WebXr*` when running the tests directly.

#### Building for Android Emulators

To build tests for an Android emulator, configure your GN args:

```gn
target_os = "android"
target_cpu = "x64"  # x64 is recommended; while arm64 emulators exist, they are very slow
enable_openxr = true  # Required on x64 Android builds prior to being enabled by default
```

Compile the test target:

```bash
autoninja -C out/Emu android_browsertests
```

#### Running on Android Emulators

Building `android_browsertests` with `enable_openxr = true` copies the
`run_xr_android_emulator_tests.py` wrapper script to the build output directory
(e.g., `out/Emu/run_xr_android_emulator_tests.py`).

The wrapper script automatically appends `--use-cmd-decoder=validating`
(required for WebGL emulation on the host OpenGL ES driver) and defaults
`--gtest_filter` to `*WebXr*`.

Full information about Android emulators can be found at
[`docs/android_emulator.md`](../../../../docs/android_emulator.md), but available
AVD configurations can be listed with:
```bash
tools/android/avd/avd.py list
```

The script intentionally does not auto-select or auto-start an emulator. You can
either:

1. **Start an emulator beforehand** using `tools/android/avd/avd.py`:
   ```bash
   tools/android/avd/avd.py start --avd-config tools/android/avd/proto/android_35_google_apis_x64.textpb
   out/Emu/run_xr_android_emulator_tests.py
   ```

2. **Pass `--avd-config` directly** to let the test runner launch and manage the
   emulator lifecycle:
   ```bash
   out/Emu/run_xr_android_emulator_tests.py --avd-config tools/android/avd/proto/android_35_google_apis_x64.textpb
   ```

3. **Filter specific tests**:
   ```bash
   out/Emu/run_xr_android_emulator_tests.py --avd-config tools/android/avd/proto/android_35_google_apis_x64.textpb --gtest_filter="WebXrVrOpenXrBrowserTest.TestMultipleEntryFromBlinkEnd"
   ```

## Writing Tests & Test Fixtures

### Test Class Names

The test classes that provide feature- and runtime-specific setup and functions
are named in the following order:

1. Feature
2. Runtime
3. "BrowserTest"
4. Optional Descriptor / Special Flags

For example, `WebXrVrOpenXrBrowserTest` is meant for testing the WebXR for VR
feature using the OpenXR runtime with standard flags enabled (i.e. the flags
required for using WebXR and the OpenXR runtime with other runtimes disabled).
`WebXrVrRuntimelessBrowserTestSensorless` on the other hand tests WebXR for VR
without any runtimes and with the orientation sensor device explicitly disabled.

In general, classes ending in "Base" should not be used directly.

### Running A Test Multiple Times With Different Runtimes

The macros provided by
[`//chrome/browser/vr/test/multi_class_browser_test.h`][multi class macros]
provide a shorthand method for running a test multiple times with different
classes/runtimes.

Most tests simply use the standard `WebXrVrOpenXrBrowserTest` class.
In this case, use the `WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F` macro, which only
takes the test name:

```cpp
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestPresentation) {
  t->LoadFileAndAwaitInitialization("test_presentation");
  t->EnterSessionWithUserGestureOrFail();
  t->EndTest();
}
```

You can also use `WEBXR_VR_ALL_RUNTIMES_PLUS_INCOGNITO_BROWSER_TEST_F` if you
want the test run in Incognito mode in addition to regular Chrome.

[multi class macros]: https://chromium.googlesource.com/chromium/src/+/main/chrome/browser/vr/test/multi_class_browser_test.h

### Restrictions & Conditional Skipping

Both instrumentation tests and browser tests have hardware/software restrictions:
for desktop browser tests, XR is supported on Windows 10 and later with a GPU that
supports DirectX 11.1, or on Android. Several tests exist that do not use XR
functionality and thus do not have these requirements.

Runtime restrictions in browser tests are handled via macros in `conditional_skipping.h`.
To add a runtime requirement to a test class, append it to `runtime_requirements_`.
The test setup will automatically skip tests that don't meet all requirements.

One-off skipping within a test can also be done using the `XR_CONDITIONAL_SKIP` macro:

```cpp
XR_CONDITIONAL_SKIP(runtime_requirements_, GetIgnoredRuntimeRequirements());
```

Bots can ignore runtime requirement checks via the `--ignore-runtime-requirements`
flag (e.g. `--ignore-runtime-requirements=DirectX_11.1` or `--ignore-runtime-requirements=*`).

### Command Line Switches

If different command line flags are needed, create a class that extends the correct
`*BrowserTestBase` and override the flags in its `SetUp` function.

### Adding New Files

If you are adding a new test or infrastructure file, consider target placement across platforms:

- **Desktop (Windows)**:
  - If the file is useful even with `enable_vr = false` (e.g. runtimeless tests), include it in `//chrome/test:xr_browser_tests_common`.
  - If it requires VR support (`enable_vr = true`), include it in `//chrome/browser/vr:xr_browser_tests_vr_required`.
- **Android**:
  - Test files are included in the `android_browsertests` target in [`//chrome/test/BUILD.gn`](https://chromium.googlesource.com/chromium/src/+/main/chrome/test/BUILD.gn).
  - Common infrastructure files belong in `:xr_browser_tests_common`.
  - OpenXR-specific browser tests are added to the `sources` list under `if (enable_openxr)` in `android_browsertests`.

## Mock Device & Input Simulation

### MockXRDeviceHookBase Lifecycle

In order to supply simulated head poses, controller buttons/axes, hand tracking data,
or runtime events (such as session lost or visibility blurred), tests instantiate an
instance of [`MockXRDeviceHookBase`][xr hook base] (or a subclass) at the start of
the test before attempting to enter XR:

```cpp
IN_PROC_BROWSER_TEST_F(WebXrVrInputBrowserTest, TestControllerInput) {
  MockXRDeviceHookBase mock_hook;
  auto& controller = mock_hook.CreateMinimalGamepad(device::mojom::XRHandedness::RIGHT);
  controller.SetTrigger(true, true, 1.0);
  ...
}
```

### Input Simulation with MockXRInputSource

Input source state is configured via [`MockXRInputSource`][mock input source].

Button identifiers (`device::XrButtonId`) and their mappings to Gamepad axes/buttonsxr
are defined in [`//device/vr/test/webxr_test_gamepad_utils.h`][gamepad utils].

[xr hook base]: https://chromium.googlesource.com/chromium/src/+/main/chrome/browser/vr/test/mock_xr_device_hook_base.h
[mock input source]: https://chromium.googlesource.com/chromium/src/+/main/chrome/browser/vr/test/mock_xr_input_source.h
[gamepad utils]: https://chromium.googlesource.com/chromium/src/+/main/device/vr/test/webxr_test_gamepad_utils.h

## Test Architecture Under the Hood

### High-Level Architecture Diagram

```xr
+---------------------------------------------------------------------------------------------------+
|  [Browser Test Process]                                                                           |
|                                                                                                   |
|  Test Case (e.g. WebXrVrInputBrowserTest)                                                         |
|         |                                                                                         |
|         v                                                                                         |
|  MockXRDeviceHookBase (implements device_test::mojom::XRTestHook)                                |
|         |  (Runs on dedicated background MockXRDeviceHookThread to avoid UI deadlocks)            |
|         |                                                                                         |
|         +-- On Windows: Passes raw mojo::ScopedMessagePipeHandle across process boundary --------+
|         |                                                                                        |
|         +-- On Android: Binds typed PendingRemote directly in-process -----------------------+   |
+---------|-------------------------------------------------------------------------------------|---+
          | (Windows: IPC)                                                                      | (Android: In-Process)
          v                                                                                     v
+-------------------------------------------------------------+                                 |
|  [Isolated XR Utility Process (Windows)]                    |                                 |
|                                                             |                                 |
|  XRDeviceService::BindHookForTesting(pipe)                  |                                 |
|         | (Type-erased ScopedMessagePipeHandle)             |                                 |
|         v                                                   |                                 |
|  OpenXrPlatformHelper::BindHookForTesting(pipe)             |                                 |
|         | (Static function pointer registered at startup)   |                                 |
|         v                                                   |                                 |
|  openxr_mock_helper.cc::BindTestHook(pipe)                  |                                 |
|         |                                                   |                                 |
+---------|---------------------------------------------------+                                 |
          |                                                                                     |
          +----------------------------------->+<-----------------------------------------------+
                                               |
                                               v
                             +-----------------------------------+
                             |  OpenXrTestHelper::Get()          |
                             |  (Embedded Mock OpenXR Runtime)   |
                             +-----------------------------------+
                                               ^
                                               | (Function dispatch table)
                             +-----------------------------------+
                             |  openxr_mock (.dll / .so)         |
                             |  [Thin Forwarding Trampoline]     |
                             +-----------------------------------+
                                               ^
                                               | (xrNegotiateLoaderRuntimeInterface)
                             +-----------------------------------+
                             |  OpenXR Loader                    |
                             +-----------------------------------+
```

### 1. The Thin Trampoline (`openxr_trampoline.cc` / `openxr_mock`)

The OpenXR loader discovers and loads an external active runtime shared library
specified by an active runtime JSON manifest:
- **Windows**: The loader discovers the manifest path via the `XR_RUNTIME_JSON`
  environment variable, which is set automatically by the test runner wrapper
  script or test fixture to point to `openxr_win.json`.
- **Android**: The loader reads the system manifest at
  `/product/etc/openxr/1/active_runtime.json` (or `/system/etc/openxr/1/active_runtime.json`),
  which is deployed from `openxr_android.json` to the device or emulator system
  directory during test setup (requiring root access on physical devices or
  `writable_system = true` in the emulator AVD config).

Rather than compiling the entire mock OpenXR runtime into a separate heavy shared
library (which previously resulted in duplicate, half-initialized `//base` singletons,
allocator mismatches, and brittle cross-DLL state that caused crashes), `device/vr:openxr_mock`
compiles [`device/vr/openxr/test/openxr_trampoline.cc`][trampoline cc] into a lightweight
(~40-line C) forwarding trampoline library (`openxr_mock.dll` on Windows,
`libopenxr_mock.so` on Android).

The trampoline exposes only two C-linkage symbols:
1. `SetMockOpenXrDispatchTable(PFN_xrGetInstanceProcAddr)`: Called by the host
   process during initialization to pass the address of the mock implementation's
   dispatch function (`GetMockXrGetInstanceProcAddr()`).
2. `xrNegotiateLoaderRuntimeInterface(...)`: Called by the OpenXR loader when
   negotiating the runtime interface, returning the stored dispatch function pointer.

All actual OpenXR mock logic lives in `fake_openxr_impl_api.cc` and
`openxr_test_helper.cc` linked directly into the host process. For more details on
the mock runtime's internal architecture, see
[`//device/vr/openxr/README.md#testing`][openxr testing].

[trampoline cc]: https://chromium.googlesource.com/chromium/src/+/main/device/vr/openxr/test/openxr_trampoline.cc
[openxr testing]: https://chromium.googlesource.com/chromium/src/+/main/device/vr/openxr/README.md#testing

### 2. The Static Registrar (`TrampolineRegistrar` in `openxr_mock_helper.cc`)

When test targets link `//device/vr:openxr_test_helper`, static initialization
instantiates `TrampolineRegistrar` in [`openxr_mock_helper.cc`][mock helper cc]:

```cpp
struct TrampolineRegistrar {
  TrampolineRegistrar() {
    device::OpenXrPlatformHelper::RegisterInitializeOpenXrMockTrampolineFn(
        &InitializeOpenXrMockTrampoline);
    device::OpenXrPlatformHelper::RegisterBindTestHookFn(&BindTestHook);
  }
};
TrampolineRegistrar g_trampoline_registrar;
```

This pattern provides two key benefits:
- **Automatic Initialization**: On the first call to `OpenXrPlatformHelper::EnsureInitialized()`,
  `OpenXrPlatformHelper` invokes the registered trampoline initialization function.
  The host process loads `openxr_mock` and passes its dispatch table via `SetMockOpenXrDispatchTable`
  automatically, without requiring tests to perform manual initialization steps.
- **Zero Production Dependencies**: Production `OpenXrPlatformHelper` only maintains
  two optional static function pointers guarded by `CHECK_IS_TEST()`, avoiding any
  build dependencies on test code in production targets.

[mock helper cc]: https://chromium.googlesource.com/chromium/src/+/main/device/vr/openxr/test/openxr_mock_helper.cc

### 3. Test-Only Mojom & Type-Erased Message Pipe

The test hook interface is defined in [`//device/vr/public/mojom/test/xr_test_hook.test-mojom`][test mojom]
under `device_test.mojom.XRTestHook` and marked **`testonly = true`**.

To prevent production services from depending on test Mojom:
- **Windows**: The production `isolated_xr_service.mojom` interface defines
  `BindHookForTesting(handle<message_pipe> receiver)`. `XRDeviceService` forwards the
  raw `mojo::ScopedMessagePipeHandle` to `OpenXrPlatformHelper::BindHookForTesting`,
  which delegates to `openxr_mock_helper.cc::BindTestHook` via the static registrar.
  `BindTestHook` unwraps the pipe handle into a typed
  `mojo::PendingRemote<device_test::mojom::XRTestHook>` and binds it to
  `OpenXrTestHelper::Get().SetTestHook(...)`.
- **Android**: Because device code runs in-process within the browser process during
  tests, `MockXRDeviceHookBase` directly passes its typed `mojo::PendingRemote` to
  `OpenXrTestHelper::Get().SetTestHook(...)`.

[test mojom]: https://chromium.googlesource.com/chromium/src/+/main/device/vr/public/mojom/test/xr_test_hook.test-mojom

### 4. Test Traits & Typemaps (`vr_public_test_typemaps`)

C++ structs used for frame verification and device configuration
(`device::ControllerFrameData`, `device::DeviceConfig`, `device::LayerData`,
`device::ViewData`) are defined in `//device/vr/public/mojom/test:vr_public_test_typemaps`
(`testonly = true`).

[`xr_test_hook_mojom_traits.h`][mojom traits] implements `mojo::StructTraits` and
`mojo::EnumTraits` mapping these C++ types directly to their Mojom equivalents in
`xr_test_hook.test-mojom`.

[mojom traits]: https://chromium.googlesource.com/chromium/src/+/main/device/vr/public/mojom/test/xr_test_hook_mojom_traits.h

### 5. Threading Model & Synchronization

Understanding the threading model is critical to writing deadlock-free tests:

- **UI Thread vs Device Thread**: The main test runner executes on the browser UI
  thread. On Android, the mock device runs in-process; on Windows, it runs in the
  isolated service process.
- **Dedicated Hook Thread (`MockXRDeviceHookThread`)**: `MockXRDeviceHookBase` spawns
  a dedicated `base::Thread` upon construction. All incoming synchronous (`[Sync]`)
  Mojo calls from the runtime (such as `WaitGetFrameData` and `OnFrameSubmitted`) are
  serviced on this thread so they never block the main browser UI thread.
- **Sequence Checkers**:
  - `main_sequence_`: Protects test setup and expectation mutations (e.g. `SetHeadPose`,
    `SimulateSessionLost`).
  - `mock_device_sequence_`: Protects queries from the mock runtime.
- **Frame Submission Synchronization**: In `MockXRDeviceHookBase::OnFrameSubmitted`,
  the synchronous Mojo reply callback is executed *before* running `wait_loop_quit_closure_`.
  This guarantees that the mock OpenXR runtime's render thread is released from its
  synchronous IPC before the test runner thread unblocks.
- **RunLoop vs WaitableEvent**: Never use `base::WaitableEvent` to block on the test
  thread, as this blocks the message pump. Always use `base::RunLoop` (or
  `WaitForTotalFrameCount`).
