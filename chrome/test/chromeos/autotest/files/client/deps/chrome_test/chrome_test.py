#!/usr/bin/env python

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import common, commands, logging, os
from autotest_lib.client.bin import utils

version = 1

def setup(top_dir):
    return


pwd = os.getcwd()
utils.update_version(pwd + '/src', False, version, setup, None)
