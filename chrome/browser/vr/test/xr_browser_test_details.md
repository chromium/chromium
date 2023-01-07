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

[xr hook base]: https://chromium.googlesource.com/chromium/src/+/main/chrome/browser/vr/test/mock_xr_device_hook_base.h
[xr service hook]: https://chromium.googlesource.com/chromium/src/+/HEAD/content/services/isolated_xr_device/xr_service_test_hook.cc
