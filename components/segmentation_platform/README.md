# Segmentation Platform

## Introduction
The segmentation platform is a platform that uses intelligence and machine learning to guide developers for building purpose-built user experience for specific segments of users.


Segmentation Platform is a layered component
(https://sites.google.com/a/chromium.org/dev/developers/design-documents/layered-components-design)
to enable it to be easily used on all platforms.

## Code structure

[components/segmentation_platform/public](.)
Public interfaces and data structure.

[components/segmentation_platform/internal](./internal)
Internal implementations.

[chrome/browser/segmentation_platform](../../chrome/browser/segmentation_platform)
Includes factories to instantiate the service.

`SegmentationPlatformService` - Public interface for segmentation platform service.

## Test models

[components/test/data/segmentation_platform](../test/data/segmentation_platform)
contains ML models used for testing.

*   `adder.tflite`: Takes two floats as input in a single tensor. Outputs a
    single tensor with a single element which is the sum of the two floats given
    as input.

## Testing

To run all the relevant C++ unit tests, you can run the `components_unittests`
target and give the `segmentation_platform` filter file as an argument:

```
./out/Default/components_unittests --test-launcher-filter-file=components/segmentation_platform/components_unittests.filter
```

To update the list of tests, you can run the following command:
```
components/segmentation_platform/tools/testing/launcher_filter_file.py
```
