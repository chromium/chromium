# Testing Cronet native API on desktop

[TOC]

## Overview

The Cronet native API is cross-platform, usable on multiple desktop and mobile
platforms.

TODO(caraitto): Add mobile test information for the native API in the
Android page as instructions for testing vary by platform.

## Checkout and build

See instructions in the [common checkout and
build](/components/cronet/build_instructions.md).

## Running tests locally

To run Cronet native API unit and integration tests:

```shell
$ gn gen out/Default  # Generate Ninja build files.
$ autoninja -C out/Default cronet_unittests cronet_tests  # Build both test suites.
$ ./out/Default/cronet_unittests  # Run unit tests.
$ ./out/Default/cronet_tests  # Run the integration tests.
```

# Running tests remotely

To test against all tryjobs:

```shell
$ git cl upload  # Upload to Gerrit.
$ git cl try  # Run the tryjob, results posted in the Gerrit review.
```

This will test against several mobile and desktop platforms, along with
special configurations like ASAN and TSAN.

You can use the -b flag to test against just one of those, like this:

```shell
$ git cl try -b linux-rel
```
