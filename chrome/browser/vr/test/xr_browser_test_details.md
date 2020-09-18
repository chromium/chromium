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
[xr service hook]: https://chromium.googlesource.com/chromium/src/+/HEAD/content/services/isolated_xr_device/xr_service_test_hook.cc

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
Since multiple wrapper classes could potentially be using the hook at once, the
hook is actually provided by acquiring a [`LockedVRTestHook`][locked hook] via
[`MixedRealityDeviceStatics::GetLockedTestHook()`][wmr statics]. This is
effectively a reference to the currently set
[`MockXRDeviceHookBase`][xr hook base] that automatically acquires and releases
a static lock on construction and destruction, making usage of the hook thread
safe.

[locked hook]:https://chromium.googlesource.com/chromium/src/+/HEAD/device/vr/test/locked_vr_test_hook.h
