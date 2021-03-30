#!/usr/bin/env python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Install the pyauto_dep dependency.

This is done by the Chrome ebuild so this in fact has nothing to do.
"""

import os
from autotest_lib.client.bin import utils

version = 1

def setup(top_dir):
    return


pwd = os.getcwd()
utils.update_version(pwd + '/src', False, version, setup, None)
