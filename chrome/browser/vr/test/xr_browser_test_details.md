# XR Browser Test Details

[TOC]

## Test Input Architecture

### Overview

When a [`MockXRDeviceHookBase`][xr hook base] is constructed, it sets itself as
the current test hook in [`XRServiceTestHook::SetTestHook()`][xr service hook].
This then causes the provided test hook to be set as the current hook for each
runtime. What specifically each runtime does after that differs (covered in each
runtime's specific section), but the important part is that the runtimes will
start calling into the test via Mojo to get controller and headset data instead
of attempting to use the real implementation.

[xr hook base]: https://chromium.googlesource.com/chromium/src/+/master/chrome/browser/vr/test/mock_xr_device_hook_base.h
[xr service hook]: https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/services/isolated_xr_device/xr_service_test_hook.cc

### OpenVR

OpenVR is handled by a mock implementation of the API found in
[`//device/vr/openvr/test/fake_openvr_impl_api.cc`][fake openvr]. This, along
with some helper files, are compiled into a standalone DLL defined in the
[`//device/vr:openvr_mock`][openvr target] target. This target is compiled
alongside the browser tests if OpenVR support is enabled, and several Windows
environment variables set during test setup. These environment variables cause
OpenVR to load the fake DLL on startup instead of the real one, which both
allows tests to provide data that appears to come from OpenVR (as opposed to
a wrapper) and prevents the need for having the real OpenVR implementation
installed.

[fake openvr]: https://chromium.googlesource.com/chromium/src/+/HEAD/device/vr/openvr/test/fake_openvr_impl_api.cc
[openvr target]: https://chromium.googlesource.com/chromium/src/+/HEAD/device/vr/BUILD.gn

When [`XRServiceTestHook::SetTestHook()`][xr service hook] is called, it in
turn calls [`OpenVRWrapper::SetTestHook()`][openvr wrapper]. This both stores
a reference to the provided hook, and if the runtime has already been started
with the fake OpenVR DLL, calls [`TestHelper::SetTestHook()`][test helper]. If
the runtime has not yet been started, [`TestHelper::SetTestHook()`][test helper]
will be called during the runtime initialization with the stored hook reference.

[openvr wrapper]: https://chromium.googlesource.com/chromium/src/+/HEAD/device/vr/openvr/openvr_api_wrapper.h
[test helper]: https://chromium.googlesource.com/chromium/src/+/HEAD/device/vr/openvr/test/test_helper.h

In either case, once the hook as been set in `TestHelper`, the Mojo setup is
complete. Anytime the fake OpenVR implementation gets a call that would require
fetching data, it calls into `TestHelper` to retrieve the data. If the hook is
set, `TestHelper` will then use it to call into the test and retrieve the set
data. If not, it returns reasonable defaults.

**Note**
Because the fake OpenVR implementation is in a separate DLL, it has a separate
heap from the rest of Chrome. Because of this, the Mojo data types returned by
the test hook cannot be used directly in the OpenVR implementation, as doing so
causes them to be destructed on the wrong heap once they go out of scope.
Instead, the data gets copied into duplicate, non-Mojo structs before being
passed back to OpenVR.

### WMR

WMR is handled by mock implementations of the wrapper classes that surround all
calls to the WMR API. The real wrappers are found in
[`//device/vr/windows_mixed_reality/wrappers/`][real wmr wrappers] while the
mocks are in
[`//device/vr/windows_mixed_reality/wrappers/test/`][mock wmr wrappers].

[real wmr wrappers]: https://chromium.googlesource.com/chromium/src/+/HEAD/device/vr/windows_mixed_reality/wrappers
[mock wmr wrappers]: https://chromium.googlesource.com/chromium/src/+/HEAD/device/vr/windows_mixed_reality/wrappers/test/

When [`XRServiceTestHook::SetTestHook()`][xr service hook] is called, it in turn
calls [`MixedRealityDeviceStatics::SetTestHook()`][wmr statics]. This stores the
reference to the hook for later use.

[wmr statics]: https://chromium.googlesource.com/chromium/src/+/HEAD/device/vr/windows_mixed_reality/mixed_reality_statics.h

When any of several WMR wrappers are created through the factories in
[`//device/vr/windows_mixed_reality/wrappers/wmr_wrapper_factories.h`][wmr factories],
they check [`MixedRealityDeviceStatics::ShouldUseMocks()`][wmr statics]. The
first time this is called, it returns true if the hook has already been set, or
false otherwise. Subsequent calls will always return the same value as the first
call did. This means that usage of the real and mock wrappers are impossible to
accidentally mix. **Note** This also means that it's currently impossible to
switch to or from the mock wrappers during a test after the runtime has been
attempted to be started.

[wmr factories]: https://chromium.googlesource.com/chromium/src/+/HEAD/device/vr/windows_mixed_reality/wrappers/wmr_wrapper_factories.h

When the mock wrappers are in use, any calls made to them that would require
data from the headset or controllers calls into the test via the test hook.
Since multiple wrapper classes could potentially be using the hook at once (as
opposed to OpenVR where all interaction is done through a single class), the
hook is actually provided by acquiring a [`LockedVRTestHook`][locked hook] via
[`MixedRealityDeviceStatics::GetLockedTestHook()`][wmr statics]. This is
effectively a reference to the currently set
[`MockXRDeviceHookBase`][xr hook base] that automatically acquires and releases
a static lock on construction and destruction, making usage of the hook thread
safe.

[locked hook]:https://chromium.googlesource.com/chromium/src/+/HEAD/device/vr/test/locked_vr_test_hook.h
