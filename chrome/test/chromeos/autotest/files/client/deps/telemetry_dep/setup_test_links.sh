#!/bin/bash
#
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script makes a symlink from the telemetry_dep to the correct place in
# the chrome_test version of the chrome source tree.

# Return an error code if the chrome_test dep isn't present
if [ ! -d /usr/local/autotest/deps/chrome_test ] ; then
  return 1
fi

if [ ! -e /usr/local/autotest/deps/chrome_test/test_src/data ]; then
  ln -sf /usr/local/autotest/deps/telemetry_dep/test_src/data \
      /usr/local/autotest/deps/chrome_test/test_src/data
fi
