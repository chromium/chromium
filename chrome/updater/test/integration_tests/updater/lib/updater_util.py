# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

if sys.platform == 'darwin':
  from test.integration_tests.updater.lib.mac.updater_util import *
elif 'win' in sys.platform:
  from test.integration_tests.updater.lib.win.updater_util import *
else:
  pass
