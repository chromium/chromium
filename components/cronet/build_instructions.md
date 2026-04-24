# Cronet build instructions

[TOC]

## Checking out the code

Follow all the
[Get the Code](https://www.chromium.org/developers/how-tos/get-the-code)
instructions for your target platform up to and including running hooks.

## Building Cronet for development and debugging

*Note: These instructions are for standard CLI environments. If you are using a
managed environment (e.g., Cider-V), please follow the platform-specific build
workflows instead of running these commands directly.*

Similarly to Chromium, to build Cronet for development and debugging purposes:

1. Use `gn` to create ninja files targeting the intended platform
1. Use `ninja` to execute the ninja files to run the build

The two main difference from a Chromium build are:

1. Cronet only builds a subset of Chromium
1. Cronet uses a different set of gn args to build

### Using gn

TODO(crbug.com/40287068): This might change in the future.
Remembering the set of gn args to be used for a Cronet build is complicated.
So, we rely on `//components/cronet/tools/cr_cronet.py` to do that for us.

```shell
$ ./components/cronet/tools/cr_cronet.py gn
```

By default, this generates the build configuration in `out/Debug-arm64`.
To better understand how this works, and the configuration parameters
it supports (e.g., `--release`, `--x64`, `--asan`),
refer to `cr_cronet.py`'s source code and:

```shell
$ ./components/cronet/tools/cr_cronet.py --help
```

### Using ninja

The previous steps generated the files needed to compile Cronet. All that
remains now is to find a target to build. This can be done through this command:

```shell
$ autoninja -C <out_dir> <target>
```

Where `<out_dir>` is what was set through `cr_cronet.py` (e.g.,
`out/Debug-arm64`) and `<target>` is one of of Cronet's target within some
`BUILD.gn` file:

- **`cronet_package`**: The complete Cronet library for Android. Artifacts land
  in `<out_dir>/cronet/` (native libraries under `libs/`).
- **`cronet_sample_apk`**: The demonstration app. APK at
  `<out_dir>/apks/CronetSample.apk`.
- **`cronet_test_instrumentation_apk`**: The Android test suite. APK at
  `<out_dir>/apks/CronetTestInstrumentation.apk`.
- **`net_unittests`**: Core Chromium network stack C++ unit tests. Native
  executable at `<out_dir>/net_unittests`.

## Building Cronet mobile for releases

To build Cronet with optimizations and with debug information stripped out:

```shell
$ gn clean <out_dir>
$ ./components/cronet/tools/cr_cronet.py gn --release
$ autoninja -C <out_dir> cronet_package
```

Default release output directory is `out/Release-arm64`.
