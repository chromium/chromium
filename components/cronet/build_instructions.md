# Cronet build instructions

[TOC]

## Checking out the code

Follow all the
[Get the Code](https://www.chromium.org/developers/how-tos/get-the-code)
instructions for your target platform up to and including running hooks.

## Building Cronet for development and debugging

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

To better understand how this works, and the configuration paraters it supports,
refer to `cr_cronet.py`'s source code and:

```shell
$ ./components/cronet/tools/cr_cronet.py --help
```

### Using ninja

The previous steps generated the files needed to compile Cronet. All that
remains now is to find a target to build. This can be done through this command:

```shell
$ autoninja -C out/your_cronet_output_directory your_cronet_target
```

Where, `your_cronet_output_directory` is what was set through `cr_cronet` and
`your_cronet_target` is one of of Cronet's target within some `BUILD.gn` file.

## Building Cronet mobile for releases

To build Cronet with optimizations and with debug information stripped out:

```shell
$ gn clean out/Release
$ ./components/cronet/tools/cr_cronet.py gn --release
$ autoninja -C out/Release cronet_package
```
