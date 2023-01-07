#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cast_project_config
import sys

from telemetry.testing import browser_test_runner

def main():
  retval = browser_test_runner.Run(cast_project_config.CONFIG, sys.argv)
  return retval


if __name__ == '__main__':
  sys.exit(main())