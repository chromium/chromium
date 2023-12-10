# Cronet build instructions

[TOC]

## Checking out the code

Follow all the
[Get the Code](https://www.chromium.org/developers/how-tos/get-the-code)
instructions for your target platform up to and including running hooks.

## Building Cronet for development and debugging

To build Cronet for development and debugging purposes:

First, `gn` is used to create ninja files targeting the intended platform, then
`ninja` executes the ninja files to run the build.

### Android builds

```shell
$ ./components/cronet/tools/cr_cronet.py gn --out_dir=out/Cronet
```

Android binaries will be built irrespective of the platform.

Note: these commands clobber output of previously executed gn commands in
`out/Cronet`. If `--out_dir` is left out, the output directory defaults to
`out/Debug` for debug builds and `out/Release` for release builds (see below).

If `--x86` option is specified, then a native library is built for Intel x86
architecture, and the output directory defaults to `out/Debug-x86` if
unspecified. This can be useful for running on mobile emulators.

### Desktop builds (targets the current OS)

TODO(caraitto): Specify how to target Chrome OS and Fuchsia.

```shell
gn gen out/Cronet
```

### Running the ninja files

Now, use the generated ninja files to execute the build against the
`cronet_package` build target:

```shell
$ ninja -C out/Cronet cronet_package
```

## Building Cronet mobile for releases

To build Cronet with optimizations and with debug information stripped out:

```shell
$ ./components/cronet/tools/cr_cronet.py gn --release
$ ninja -C out/Release cronet_package
```

Note: these commands clobber output of previously executed gn commands in
`out/Release`.

## Building for other architectures

By default ARMv7 32-bit executables are generated. To generate executables
targeting other architectures modify [cr_cronet.py](tools/cr_cronet.py)'s
`gn_args` variable to include:

*   For ARMv8 64-bit: `target_cpu="arm64"`
*   For x86 32-bit: `target_cpu="x86"`
*   For x86 64-bit: `target_cpu="x64"`

Alternatively you can run `gn args {out_dir}` and modify arguments in the editor
that comes up. This has advantage of not changing `cr_cronet.py`.
