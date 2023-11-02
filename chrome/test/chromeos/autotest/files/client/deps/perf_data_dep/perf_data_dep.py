#!/usr/bin/env python

# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Install the perf_data_dep dependency.

This is done by the Chrome ebuild so this in fact has nothing to do.
"""

import os
from autotest_lib.client.bin import utils

version = 1

def setup(top_dir):
    return


pwd = os.getcwd()
utils.update_version(pwd + '/src', False, version, setup, None)
