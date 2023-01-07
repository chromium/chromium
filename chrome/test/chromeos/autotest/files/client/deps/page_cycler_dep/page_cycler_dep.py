#!/usr/bin/env python

# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Install the page_cycler_dep dependency.
"""

import os
from autotest_lib.client.bin import utils

version = 1

def setup(top_dir):
    return


pwd = os.getcwd()
utils.update_version(pwd + '/src', False, version, setup, pwd)
