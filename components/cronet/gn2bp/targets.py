# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEFAULT_TARGETS = [
    "//components/cronet/android:gn2bp_production_targets",
]

DEFAULT_TESTS = [
    "//components/cronet/android:gn2bp_test_targets",
]

# Usually, README.chromium lives next to the BUILD.gn. However, some cases are
# different, this dictionary allows setting a specific README.chromium path
# for a specific BUILD.gn
README_MAPPING = {
    # Moving is undergoing, see crbug/40273848
    "buildtools/third_party/libc++": "third_party/libc++",
    # Moving is undergoing, see crbug/40273848
    "buildtools/third_party/libc++abi": "third_party/libc++abi",
}
