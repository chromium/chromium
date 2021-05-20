# XR Browser Tests

[TOC]

## Introduction

This documentation concerns `xr_browser_test.h`, `xr_browser_test.cc`, and files
that use them or their subclasses.

These files port the framework used by XR instrumentation tests (located in
[`//chrome/android/javatests/src/org/chromium/chrome/browser/vr/`][vr android dir]
and documented in
`//chrome/android/javatests/src/org/chromium/chrome/browser/vr/*.md`) for
use in browser tests in order to test XR features on desktop platforms.

[vr android dir]: https://chromium.googlesource.com/chromium/src/+/main/chrome/android/javatests/src/org/chromium/chrome/browser/vr

This is pretty much a direct port, with the same JavaScript/HTML files being
used for both and the Java/C++ code being functionally equivalent to each other,
so the instrumentation test's documentation on writing tests using the framework
is applicable here, too. As such, this documentation only covers any notable
differences between the two implementations.

## Restrictions

Both the instrumentation tests and browser tests have hardware/software
restrictions - in the case of browser tests, XR is only supported on Windows 8
and later (or Windows 7 with a non-standard patch applied) with a GPU that
supports DirectX 11.1, although several tests exist that don't actually use XR
functionality, and thus don't have these requirements.

Runtime restrictions in browser tests are handled via the macros in
`conditional_skipping.h`. To add a runtime requirement to a test class, simply
append it to the `runtime_requirements_` vector that each class has. The
test setup will automatically skip tests that don't meet all requirements.

One-off skipping within a test can also be done by using the XR_CONDITIONAL_SKIP
macro directly in a test.

The bots can be made to ignore these runtime requirement checks if we expect
the requirements to always be met (and thus we want the tests to fail if they
aren't) via the `--ignore-runtime-requirements` argument. This takes a
comma-separated list of requirements to ignore, or the wildcard (\*) to ignore
all requirements. For example, `--ignore-runtime-requirements=DirectX_11.1`
would cause a test that requires a DirectX 11.1 device to be run even if a
suitable device is not found.

New requirements can be added by adding to the `XrTestRequirement` enum in
`conditional_skipping.h` and adding its associated checking logic in
`conditional_skipping.cc`.

## Command Line Switches

Instrumentation tests are able to add and remove command line switches on a
per-test-case basis using `@CommandLine` annotations, but equivalent
functionality does not exist in browser tests.

Instead, if different command line flags are needed, a new class will need to
be created that extends the correct type of `*BrowserTestBase` and overrides the
flags that are set in its `SetUp` function.

## Compiling And Running

The tests are compiled in the `xr_browser_tests` target. This is a combination
of the `xr_browser_tests_binary` target, which is the actual test, and the
`xr_browser_tests_runner` target, which is a wrapper script that ensures special
setup is completed before running the tests.

Once compiled, the tests can be run using the following command line:

`run_xr_browser_tests.py --enable-gpu --test-launcher-jobs=1
--enable-pixel-output-in-tests`

Additional options such as test filtering can be found by running
`xr_browser_tests.exe --help` and `xr_browser_tests.exe --gtest_help`.

Because the "test" is actually a Python wrapper script, you may need to prepend
`python` to the front of the command on Windows if Python file association is
not set up on your machine.

## Adding New Files

If you are adding a new test or infrastructure file to the target, you'll need
to consider whether it's useful with the `enable_vr` gn arg set to false or not.
If it is, then it should be included in `//chrome/test:xr_browser_tests_common`,
otherwise it should be included in
`//chrome/browser/vr:xr_browser_tests_vr_required`.

If including in `//chrome/test:xr_browser_tests_common`, you may need to hide
some VR-specific functionality in the file behind `#if BUILDFLAG(ENABLE_VR)`.

## Running A Test Multiple Times With Different Runtimes

The macros provided by
[`//chrome/browser/vr/test/multi_class_browser_test.h`][multi class macros]
provide a shorthand method for running a test multiple times with different
classes/runtimes. This is effectively the same as declaring some implementation
function that takes a reference to some base class shared by all the subclasses
you want to run the test with, then having each test call that implementation.

These macros help cut down on boilerplate code, but if you need either:

1. Class-specific setup before running the implementation
2. Different test logic in the implementation depending on the provided class

You should consider using the standard IN_PROC_BROWSER_TEST_F macros instead.
Small snippets of runtime-specific code are acceptable, but if it affects
readability significantly, the tests should probably remain separate.

Most tests simply use the standard `WebXrVrOpenXrBrowserTest` class.
In this case, you can instead use the `WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F`
macro, which only needs to take the test name, further cutting down on
boilerplate code.

You can also use `WEBXR_VR_ALL_RUNTIMES_PLUS_INCOGNITO_BROWSER_TEST_F` if you
want the same functionality as `WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F`, but
also want the test run in Incognito mode in addition to regular Chrome.

[multi class macros]: https://chromium.googlesource.com/chromium/src/+/main/chrome/browser/vr/test/multi_class_browser_test.h

## Test Class Names

The test classes that are used to provide feature and runtime-specific setup and
functions are named in the following order:

1. Feature
2. Runtime
3. "BrowserTest"
4. Optional Descriptor/special flags

For example, `WebXrVrOpenXrBrowserTest` is meant for testing the WebXR for VR
feature using the OpenXR runtime with standard flags enabled, i.e. the flags
required for using WebXR and the OpenXR runtime with other runtimes disabled.
`WebXrVrRuntimelessBrowserTestSensorless` on the other hand would be for
testing WebVR for VR without any runtimes and with the orientation sensor
device explicitly disabled.

In general, classes ending in "Base" should not be used directly.

## Controller and Head Input

The XR browser tests provide a way to plumb controller and headset data (e.g.
currently touched/pressed buttons and poses) from the test through the runtime
being tested. Details about what goes on under the hood can be found in
[`//chrome/browser/vr/test/xr_browser_test_details.md`][xr details], but below
is a quick guide on how to use them.

[xr details]: https://chromium.googlesource.com/chromium/src/+/main/chrome/browser/vr/test/xr_browser_test_details.md

In order to let a test provide data to a runtime, it must create an instance of
[`MockXRDeviceHookBase`][xr hook base] or some subclass of it. This should be
created at the beginning of the test before any attempts to enter VR are made,
as there are currently assumptions that prevent switching to or from the mock
runtimes once they have been attempted to be started.

[xr hook base]: https://chromium.googlesource.com/chromium/src/+/main/chrome/browser/vr/test/mock_xr_device_hook_base.h

Once created, the runtime being used will call the various functions inherited
from [`VRTestHook`][vr test hook] whenever it would normally acquire or submit
data from or to an actual device. For example, `WaitGetControllerData()` will be
called every time the runtime would normally check the state of a real
controller, and `OnFrameSubmitted()` will be called each time the runtime
submits a finished frame to the headset.

[vr test hook]: https://chromium.googlesource.com/chromium/src/+/main/device/vr/test/test_hook.h

For real examples on how to use the input capabilities, look at the tests in
[`//chrome/browser/vr/webxr_vr_input_browser_test.cc`][input test].

[input test]: https://chromium.googlesource.com/chromium/src/+/main/chrome/browser/vr/webxr_vr_input_browser_test.cc
